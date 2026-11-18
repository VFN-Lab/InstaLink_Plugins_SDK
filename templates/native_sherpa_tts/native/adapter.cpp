/**
 * adapter.cpp - libsherpa_tts_adapter.so
 *
 * JNI / C adapter that bridges the InstaLink native-runtime host to the
 * sherpa-onnx offline TTS engine (VITS models).
 *
 * Exported C symbols (stable InstaLink ABI):
 *   void        instalink_extension_on_load(const char *runtime_root)
 *   void        instalink_extension_on_unload()
 *   const char *instalink_extension_describe()
 *   const char *instalink_extension_invoke(const char *action, const char *payload_json)
 *   void        instalink_extension_free_string(const char *value)
 *
 * Commands (JSON in / JSON out):
 *   init        — load model, return sample_rate + num_speakers
 *   synthesize  — generate speech, write WAV, return audio_path
 *   stop        — abort in-progress synthesis
 *   get_info    — engine metadata
 *   inspect     — supported commands list
 *
 * Build: Android NDK r27+, minSdk 24, C++17
 */

#include <android/log.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include <sys/stat.h>

#include "sherpa_onnx_api.h"

/* Marks a symbol as part of the public ABI even under -fvisibility=hidden */
#define TTS_API __attribute__((visibility("default")))

/* ── Logging ──────────────────────────────────────────────────────────────── */
#define TAG  "TtsAdapter"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

/* ── Global state ─────────────────────────────────────────────────────────── */
static const SherpaOnnxOfflineTts *g_tts      = nullptr;
static std::mutex              g_tts_mutex;
static std::atomic<bool>       g_stop        {false};
static std::string             g_root_dir;
static int32_t                 g_sample_rate  = 0;
static int32_t                 g_num_speakers = 0;
static bool                    g_initialized  = false;

/* ══════════════════════════════════════════════════════════════════════════
 *  Minimal JSON helpers
 *  (no external deps — payloads are simple flat objects we control)
 * ══════════════════════════════════════════════════════════════════════════ */

static std::string json_get_string(const std::string &j, const std::string &key,
                                   const std::string &def = "") {
    const std::string needle = "\"" + key + "\"";
    size_t p = j.find(needle);
    if (p == std::string::npos) return def;
    p += needle.size();
    while (p < j.size() && (j[p] == ' ' || j[p] == '\t' || j[p] == ':')) ++p;
    if (p >= j.size() || j[p] != '"') return def;
    ++p;
    std::string r;
    while (p < j.size() && j[p] != '"') {
        if (j[p] == '\\' && p + 1 < j.size()) {
            ++p;
            switch (j[p]) {
                case '"':  r += '"';  break;
                case '\\': r += '\\'; break;
                case 'n':  r += '\n'; break;
                case 'r':  r += '\r'; break;
                case 't':  r += '\t'; break;
                default:   r += j[p]; break;
            }
        } else {
            r += j[p];
        }
        ++p;
    }
    return r;
}

static int32_t json_get_int(const std::string &j, const std::string &key,
                             int32_t def = 0) {
    const std::string needle = "\"" + key + "\"";
    size_t p = j.find(needle);
    if (p == std::string::npos) return def;
    p += needle.size();
    while (p < j.size() && (j[p] == ' ' || j[p] == '\t' || j[p] == ':')) ++p;
    if (p >= j.size()) return def;
    return (int32_t)strtol(j.c_str() + p, nullptr, 10);
}

static float json_get_float(const std::string &j, const std::string &key,
                              float def = 0.0f) {
    const std::string needle = "\"" + key + "\"";
    size_t p = j.find(needle);
    if (p == std::string::npos) return def;
    p += needle.size();
    while (p < j.size() && (j[p] == ' ' || j[p] == '\t' || j[p] == ':')) ++p;
    if (p >= j.size()) return def;
    return strtof(j.c_str() + p, nullptr);
}

static bool json_get_bool(const std::string &j, const std::string &key,
                           bool def = false) {
    const std::string needle = "\"" + key + "\"";
    size_t p = j.find(needle);
    if (p == std::string::npos) return def;
    p += needle.size();
    while (p < j.size() && (j[p] == ' ' || j[p] == '\t' || j[p] == ':')) ++p;
    if (p >= j.size()) return def;
    return j.compare(p, 4, "true") == 0;
}

static std::string json_escape(const std::string &s) {
    std::string r;
    r.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n";  break;
            case '\r': r += "\\r";  break;
            case '\t': r += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    r += buf;
                } else {
                    r += (char)c;
                }
        }
    }
    return r;
}

static std::string json_ok(const std::string &extras = "") {
    std::string r = "{\"status\":\"ok\"";
    if (!extras.empty()) { r += ','; r += extras; }
    r += '}';
    return r;
}

static std::string json_error(const std::string &msg) {
    return "{\"status\":\"error\",\"message\":\"" + json_escape(msg) + "\"}";
}

/* ══════════════════════════════════════════════════════════════════════════
 *  WAV writer  (IEEE float → 16-bit PCM WAV)
 * ══════════════════════════════════════════════════════════════════════════ */

static bool write_wav(const std::string &path,
                      const float *samples, int32_t n, int32_t rate) {
    FILE *f = fopen(path.c_str(), "wb");
    if (!f) {
        LOGE("write_wav: cannot open %s (errno=%d)", path.c_str(), errno);
        return false;
    }

    const int32_t channels   = 1;
    const int32_t bps        = 16;
    const int32_t byte_rate  = rate * channels * bps / 8;
    const int16_t blk_align  = (int16_t)(channels * bps / 8);
    const int32_t data_sz    = n * blk_align;
    const int32_t chunk_sz   = 36 + data_sz;

    /* RIFF */
    fwrite("RIFF", 1, 4, f);
    fwrite(&chunk_sz,  4, 1, f);
    fwrite("WAVE", 1, 4, f);
    /* fmt  */
    fwrite("fmt ", 1, 4, f);
    int32_t fmt_sz = 16;      fwrite(&fmt_sz,    4, 1, f);
    int16_t fmt    = 1;       fwrite(&fmt,        2, 1, f); /* PCM */
    int16_t chans  = (int16_t)channels; fwrite(&chans, 2, 1, f);
    fwrite(&rate,      4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&blk_align, 2, 1, f);
    int16_t bps16  = (int16_t)bps; fwrite(&bps16,  2, 1, f);
    /* data */
    fwrite("data", 1, 4, f);
    fwrite(&data_sz,   4, 1, f);

    std::vector<int16_t> pcm(n);
    for (int32_t i = 0; i < n; ++i) {
        float s = samples[i];
        if (s >  1.0f) s =  1.0f;
        if (s < -1.0f) s = -1.0f;
        pcm[i] = static_cast<int16_t>(s * 32767.0f);
    }
    fwrite(pcm.data(), sizeof(int16_t), (size_t)n, f);
    fclose(f);
    return true;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Synthesis callback — returns 0 (abort) when stop is requested
 * ══════════════════════════════════════════════════════════════════════════ */

static int32_t synth_callback(const float * /*samples*/,
                              int32_t       /*n*/) {
    return g_stop.load() ? 0 : 1;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Helpers
 * ══════════════════════════════════════════════════════════════════════════ */

static void ensure_dir(const std::string &dir) {
    struct stat st{};
    if (stat(dir.c_str(), &st) != 0) {
        mkdir(dir.c_str(), 0755);
    }
}

static bool path_exists(const std::string &path) {
    struct stat st{};
    return !path.empty() && stat(path.c_str(), &st) == 0;
}

/* Resolve a path: absolute → use as-is; relative → prepend root_dir */
static std::string resolve(const std::string &p) {
    if (p.empty() || p[0] == '/') return p;
    return g_root_dir + "/" + p;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Command handlers
 * ══════════════════════════════════════════════════════════════════════════ */

/* ── init ─────────────────────────────────────────────────────────────────── */
static std::string cmd_init(const std::string &payload) {
    std::lock_guard<std::mutex> lock(g_tts_mutex);

    /* Tear down any existing engine */
    if (g_tts) {
        SherpaOnnxDestroyOfflineTts(g_tts);
        g_tts         = nullptr;
        g_initialized = false;
    }

    /* Parse payload */
    const std::string model_path  = json_get_string(payload, "model_path",  "models/vits-model.onnx");
    const std::string tokens_path = json_get_string(payload, "tokens",      "models/tokens.txt");
    const std::string lexicon_path= json_get_string(payload, "lexicon",     "");
    const std::string data_dir    = json_get_string(payload, "data_dir",    "");
    const int32_t     num_threads = json_get_int   (payload, "num_threads", 2);
    const bool        debug       = json_get_bool  (payload, "debug",       false);

    const std::string model_abs   = resolve(model_path);
    const std::string tokens_abs  = resolve(tokens_path);
    const std::string lexicon_abs = lexicon_path.empty() ? "" : resolve(lexicon_path);
    const std::string data_abs    = data_dir.empty()     ? "" : resolve(data_dir);
    const float       silence     = json_get_float(payload, "silence_scale", 0.0f);

    if (!path_exists(model_abs)) {
        return json_error("Model file not found: " + model_abs);
    }
    if (!path_exists(tokens_abs)) {
        return json_error("Tokens file not found: " + tokens_abs);
    }
    if (!data_abs.empty() && !path_exists(data_abs)) {
        return json_error("Data directory not found: " + data_abs);
    }

    const char *version = SherpaOnnxGetVersionStr();
    const char *git_sha = SherpaOnnxGetGitSha1();
    LOGI("init: sherpa=%s sha=%s model=%s tokens=%s data=%s threads=%d",
         version ? version : "unknown",
         git_sha ? git_sha : "unknown",
         model_abs.c_str(),
         tokens_abs.c_str(),
         data_abs.empty() ? "<null>" : data_abs.c_str(),
         num_threads);

    SherpaOnnxOfflineTtsVitsModelConfig vits{};
    vits.model         = model_abs.c_str();
    vits.tokens        = tokens_abs.c_str();
    vits.lexicon       = lexicon_abs.empty() ? nullptr : lexicon_abs.c_str();
    vits.data_dir      = data_abs.empty() ? nullptr : data_abs.c_str();
    vits.noise_scale   = 0.667f;
    vits.noise_scale_w = 0.8f;
    vits.length_scale  = 1.0f;
    vits.dict_dir      = nullptr;

    SherpaOnnxOfflineTtsModelConfig model_cfg{};
    model_cfg.vits        = vits;
    model_cfg.num_threads = num_threads;
    model_cfg.debug       = debug ? 1 : 0;
    model_cfg.provider    = "cpu";

    SherpaOnnxOfflineTtsConfig config{};
    config.model             = model_cfg;
    config.rule_fsts         = nullptr;
    config.max_num_sentences = 1;
    config.rule_fars         = nullptr;
    config.silence_scale     = silence;

    g_tts = SherpaOnnxCreateOfflineTts(&config);
    if (!g_tts) {
        LOGE("init: SherpaOnnxCreateOfflineTts returned null");
        return json_error("Failed to create TTS engine — check model paths");
    }

    g_sample_rate   = SherpaOnnxOfflineTtsSampleRate  (g_tts);
    g_num_speakers  = SherpaOnnxOfflineTtsNumSpeakers (g_tts);
    g_initialized   = true;

    LOGI("init: ready  sample_rate=%d  num_speakers=%d", g_sample_rate, g_num_speakers);

    std::ostringstream e;
    e << "\"sample_rate\":"   << g_sample_rate
      << ",\"num_speakers\":" << g_num_speakers;
    return json_ok(e.str());
}

/* ── synthesize ───────────────────────────────────────────────────────────── */
static std::string cmd_synthesize(const std::string &payload) {
    if (!g_initialized || !g_tts) {
        return json_error("Engine not initialized — call init first");
    }

    const std::string text       = json_get_string(payload, "text",        "");
    const int32_t     speaker_id = json_get_int   (payload, "speaker_id",  0);
    const float       speed      = json_get_float (payload, "speed",       1.0f);
    std::string       out_path   = json_get_string(payload, "output_path", "");

    if (text.empty()) {
        return json_error("\"text\" field is required");
    }

    /* Derive output path if not supplied */
    if (out_path.empty()) {
        const std::string cache_dir = g_root_dir + "/cache";
        ensure_dir(cache_dir);
        out_path = cache_dir + "/tts_output.wav";
    } else {
        out_path = resolve(out_path);
        const size_t slash = out_path.find_last_of('/');
        if (slash != std::string::npos) {
            ensure_dir(out_path.substr(0, slash));
        }
    }

    g_stop.store(false);

    LOGI("synthesize: len=%zu sid=%d speed=%.2f → %s",
         text.size(), speaker_id, (double)speed, out_path.c_str());

    const auto t0 = std::chrono::steady_clock::now();

    const SherpaOnnxGeneratedAudio *audio = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_tts_mutex);
        audio = SherpaOnnxOfflineTtsGenerateWithCallback(
                    g_tts, text.c_str(), speaker_id, speed, synth_callback);
    }

    if (!audio) {
        if (g_stop.load()) return json_error("Synthesis stopped by user");
        return json_error("Synthesis failed — engine returned null");
    }
    if (audio->n == 0) {
        SherpaOnnxDestroyOfflineTtsGeneratedAudio(audio);
        return json_error("Synthesis produced zero samples");
    }

    const auto  t1         = std::chrono::steady_clock::now();
    const int64_t dur_ms   = std::chrono::duration_cast<
                                 std::chrono::milliseconds>(t1 - t0).count();
    const int32_t n        = audio->n;
    const int32_t sr       = audio->sample_rate;
    const bool    ok       = write_wav(out_path, audio->samples, n, sr);

    SherpaOnnxDestroyOfflineTtsGeneratedAudio(audio);

    if (!ok) {
        return json_error("Failed to write WAV: " + out_path);
    }

    std::ostringstream e;
    e << "\"audio_path\":\""  << json_escape(out_path) << "\""
      << ",\"num_samples\":"  << n
      << ",\"sample_rate\":"  << sr
      << ",\"duration_ms\":"  << dur_ms;
    return json_ok(e.str());
}

/* ── stop ─────────────────────────────────────────────────────────────────── */
static std::string cmd_stop(const std::string & /*payload*/) {
    g_stop.store(true);
    LOGI("stop requested");
    return json_ok();
}

/* ── get_info ─────────────────────────────────────────────────────────────── */
static std::string cmd_get_info(const std::string & /*payload*/) {
    const char *version = SherpaOnnxGetVersionStr();
    const char *git_sha = SherpaOnnxGetGitSha1();
    std::ostringstream e;
    e << "\"engine\":\"sherpa-onnx\""
      << ",\"backend\":\"offline-tts-vits\""
      << ",\"sherpa_version\":\"" << json_escape(version ? version : "unknown") << "\""
      << ",\"sherpa_git_sha\":\"" << json_escape(git_sha ? git_sha : "unknown") << "\""
      << ",\"initialized\":"  << (g_initialized ? "true" : "false")
      << ",\"sample_rate\":"  << g_sample_rate
      << ",\"num_speakers\":" << g_num_speakers
      << ",\"root_dir\":\""   << json_escape(g_root_dir) << "\""
      << ",\"abi\":\""
#if   defined(__aarch64__)
      << "arm64-v8a"
#elif defined(__arm__)
      << "armeabi-v7a"
#elif defined(__x86_64__)
      << "x86_64"
#elif defined(__i386__)
      << "x86"
#else
      << "unknown"
#endif
      << "\"";
    return json_ok(e.str());
}

/* ── inspect ──────────────────────────────────────────────────────────────── */
static std::string cmd_inspect(const std::string & /*payload*/) {
    const char *version = SherpaOnnxGetVersionStr();
    std::ostringstream e;
    e << "\"commands\":[\"init\",\"synthesize\",\"stop\",\"get_info\",\"inspect\"]"
      << ",\"version\":\"1.0.0\""
      << ",\"sherpa_version\":\"" << json_escape(version ? version : "unknown") << "\""
      << ",\"adapter\":\"libsherpa_tts_adapter.so\""
      << ",\"initialized\":" << (g_initialized ? "true" : "false");
    return json_ok(e.str());
}

/* ── dispatcher ───────────────────────────────────────────────────────────── */
static std::string dispatch(const std::string &cmd, const std::string &payload) {
    LOGI("dispatch: %s", cmd.c_str());
    if      (cmd == "init")       return cmd_init      (payload);
    else if (cmd == "synthesize") return cmd_synthesize(payload);
    else if (cmd == "stop")       return cmd_stop      (payload);
    else if (cmd == "get_info")   return cmd_get_info  (payload);
    else if (cmd == "inspect")    return cmd_inspect   (payload);
    else return json_error("Unknown command: " + cmd);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Stable InstaLink exports
 * ══════════════════════════════════════════════════════════════════════════ */
extern "C" {

TTS_API void instalink_extension_on_load(const char *runtime_root) {
    if (runtime_root) g_root_dir = runtime_root;
    LOGI("instalink_extension_on_load: root=%s", g_root_dir.c_str());
}

TTS_API void instalink_extension_on_unload() {
    LOGI("instalink_extension_on_unload");
    std::lock_guard<std::mutex> lock(g_tts_mutex);
    if (g_tts) {
        SherpaOnnxDestroyOfflineTts(g_tts);
        g_tts         = nullptr;
        g_initialized = false;
    }
}

TTS_API const char *instalink_extension_describe() {
    return strdup(cmd_inspect("{}").c_str());
}

TTS_API const char *instalink_extension_invoke(const char *command, const char *payload_json) {
    if (!command) {
        return strdup("{\"status\":\"error\",\"message\":\"null command\"}");
    }
    const std::string result = dispatch(
        std::string(command),
        payload_json ? std::string(payload_json) : "{}");
    return strdup(result.c_str());
}

TTS_API void instalink_extension_free_string(const char *str) {
    free(const_cast<char *>(str));
}

} /* extern "C" */
