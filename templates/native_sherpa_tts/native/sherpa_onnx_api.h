// sherpa-onnx TTS C API subset for InstaLink native_echo_lab.
// This header mirrors the newer offline TTS layout used by the bundled
// Android runtime libraries, including matcha/kokoro/kitten/zipvoice/pocket/
// supertonic fields so struct offsets match the shipped .so files.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct SherpaOnnxOfflineTtsVitsModelConfig {
    const char *model;
    const char *lexicon;
    const char *tokens;
    const char *data_dir;
    float noise_scale;
    float noise_scale_w;
    float length_scale;
    const char *dict_dir;
} SherpaOnnxOfflineTtsVitsModelConfig;

typedef struct SherpaOnnxOfflineTtsMatchaModelConfig {
    const char *acoustic_model;
    const char *vocoder;
    const char *lexicon;
    const char *tokens;
    const char *data_dir;
    float noise_scale;
    float length_scale;
    const char *dict_dir;
} SherpaOnnxOfflineTtsMatchaModelConfig;

typedef struct SherpaOnnxOfflineTtsKokoroModelConfig {
    const char *model;
    const char *voices;
    const char *tokens;
    const char *data_dir;
    float length_scale;
    const char *dict_dir;
    const char *lexicon;
    const char *lang;
} SherpaOnnxOfflineTtsKokoroModelConfig;

typedef struct SherpaOnnxOfflineTtsKittenModelConfig {
    const char *model;
    const char *voices;
    const char *tokens;
    const char *data_dir;
    float length_scale;
} SherpaOnnxOfflineTtsKittenModelConfig;

typedef struct SherpaOnnxOfflineTtsZipvoiceModelConfig {
    const char *tokens;
    const char *encoder;
    const char *decoder;
    const char *vocoder;
    const char *data_dir;
    const char *lexicon;
    float feat_scale;
    float t_shift;
    float target_rms;
    float guidance_scale;
} SherpaOnnxOfflineTtsZipvoiceModelConfig;

typedef struct SherpaOnnxOfflineTtsPocketModelConfig {
    const char *lm_flow;
    const char *lm_main;
    const char *encoder;
    const char *decoder;
    const char *text_conditioner;
    const char *vocab_json;
    const char *token_scores_json;
    int32_t voice_embedding_cache_capacity;
} SherpaOnnxOfflineTtsPocketModelConfig;

typedef struct SherpaOnnxOfflineTtsSupertonicModelConfig {
    const char *duration_predictor;
    const char *text_encoder;
    const char *vector_estimator;
    const char *vocoder;
    const char *tts_json;
    const char *unicode_indexer;
    const char *voice_style;
} SherpaOnnxOfflineTtsSupertonicModelConfig;

typedef struct SherpaOnnxOfflineTtsModelConfig {
    SherpaOnnxOfflineTtsVitsModelConfig vits;
    int32_t num_threads;
    int32_t debug;
    const char *provider;
    SherpaOnnxOfflineTtsMatchaModelConfig matcha;
    SherpaOnnxOfflineTtsKokoroModelConfig kokoro;
    SherpaOnnxOfflineTtsKittenModelConfig kitten;
    SherpaOnnxOfflineTtsZipvoiceModelConfig zipvoice;
    SherpaOnnxOfflineTtsPocketModelConfig pocket;
    SherpaOnnxOfflineTtsSupertonicModelConfig supertonic;
} SherpaOnnxOfflineTtsModelConfig;

typedef struct SherpaOnnxOfflineTtsConfig {
    SherpaOnnxOfflineTtsModelConfig model;
    const char *rule_fsts;
    int32_t max_num_sentences;
    const char *rule_fars;
    float silence_scale;
} SherpaOnnxOfflineTtsConfig;

typedef struct SherpaOnnxGeneratedAudio {
    const float *samples;
    int32_t n;
    int32_t sample_rate;
} SherpaOnnxGeneratedAudio;

typedef struct SherpaOnnxOfflineTts SherpaOnnxOfflineTts;

typedef int32_t (*SherpaOnnxGeneratedAudioCallback)(const float *samples, int32_t n);

const char *SherpaOnnxGetVersionStr(void);
const char *SherpaOnnxGetGitSha1(void);

const SherpaOnnxOfflineTts *SherpaOnnxCreateOfflineTts(
    const SherpaOnnxOfflineTtsConfig *config);
void SherpaOnnxDestroyOfflineTts(const SherpaOnnxOfflineTts *tts);
int32_t SherpaOnnxOfflineTtsNumSpeakers(const SherpaOnnxOfflineTts *tts);
int32_t SherpaOnnxOfflineTtsSampleRate(const SherpaOnnxOfflineTts *tts);

const SherpaOnnxGeneratedAudio *SherpaOnnxOfflineTtsGenerate(
    const SherpaOnnxOfflineTts *tts,
    const char *text,
    int32_t sid,
    float speed);

const SherpaOnnxGeneratedAudio *SherpaOnnxOfflineTtsGenerateWithCallback(
    const SherpaOnnxOfflineTts *tts,
    const char *text,
    int32_t sid,
    float speed,
    SherpaOnnxGeneratedAudioCallback callback);

void SherpaOnnxDestroyOfflineTtsGeneratedAudio(const SherpaOnnxGeneratedAudio *p);

#ifdef __cplusplus
}
#endif
