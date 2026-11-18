#include <android/log.h>
#include <android/native_window.h>
#include "instalink_extension_api.h"

#include <string>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>

#define LOG_TAG "SampleGame"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace {

char* DupString(const std::string& value) {
    char* buffer = static_cast<char*>(std::malloc(value.size() + 1));
    if (!buffer) return nullptr;
    std::memcpy(buffer, value.c_str(), value.size() + 1);
    return buffer;
}

uint32_t PackColor(uint8_t r, uint8_t g, uint8_t b) {
    return (255 << 24) | (b << 16) | (g << 8) | r;
}

class SimpleRenderer {
public:
    void Start(ANativeWindow* window, int width, int height) {
        std::lock_guard<std::mutex> lock(mtx);
        this->window = window;
        this->width = width;
        this->height = height;
        
        if (window) {
            ANativeWindow_acquire(window);
            ANativeWindow_setBuffersGeometry(window, width, height, WINDOW_FORMAT_RGBA_8888);
            running = true;
            renderThread = std::thread([this]() { RenderLoop(); });
            LOGI("Renderer started: %dx%d", width, height);
        }
    }

    void Stop() {
        running = false;
        if (renderThread.joinable()) {
            renderThread.join();
        }
        std::lock_guard<std::mutex> lock(mtx);
        if (window) {
            ANativeWindow_release(window);
            window = nullptr;
        }
        LOGI("Renderer stopped");
    }

    void HandleInput(double x, double y) {
        std::lock_guard<std::mutex> lock(mtx);
        touchX = x;
        touchY = y;
        isTouched = true;
        // Reset after 300ms
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            isTouched = false;
        }).detach();
    }

private:
    void RenderLoop() {
        float hue = 0.0f;
        while (running) {
            DrawFrame(hue);
            hue += 0.01f;
            if (hue > 1.0f) hue -= 1.0f;
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps
        }
    }

    void DrawFrame(float hue) {
        std::lock_guard<std::mutex> lock(mtx);
        if (!window) return;

        ANativeWindow_Buffer buffer;
        if (ANativeWindow_lock(window, &buffer, nullptr) != 0) {
            return;
        }

        uint32_t* pixels = static_cast<uint32_t*>(buffer.bits);
        int stride = buffer.stride;
        
        // Background color
        uint32_t bgColor = PackColor(15, 23, 42); // Dark blue/slate
        
        // Fill background
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                pixels[y * stride + x] = bgColor;
            }
        }

        // Draw touch indicator
        if (isTouched) {
            int cx = static_cast<int>(touchX);
            int cy = static_cast<int>(touchY);
            int radius = 50;
            uint32_t touchColor = PackColor(56, 189, 248); // Light blue
            
            for (int y = std::max(0, cy - radius); y < std::min(height, cy + radius); ++y) {
                for (int x = std::max(0, cx - radius); x < std::min(width, cx + radius); ++x) {
                    int dx = x - cx;
                    int dy = y - cy;
                    if (dx*dx + dy*dy <= radius*radius) {
                        pixels[y * stride + x] = touchColor;
                    }
                }
            }
        }

        ANativeWindow_unlockAndPost(window);
    }

    std::mutex mtx;
    ANativeWindow* window = nullptr;
    int width = 0, height = 0;
    std::thread renderThread;
    std::atomic<bool> running{false};
    
    std::atomic<bool> isTouched{false};
    std::atomic<double> touchX{0};
    std::atomic<double> touchY{0};
};

SimpleRenderer gRenderer;

} // namespace

extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_on_load(const char*) {}
extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_on_unload() {}

extern "C" INSTALINK_EXTENSION_EXPORT const char* instalink_extension_describe() {
    return DupString(R"({"ok":true,"adapter":"sample_game","immersive":true})");
}

extern "C" INSTALINK_EXTENSION_EXPORT const char* instalink_extension_invoke(const char* action, const char* payload) {
    return DupString(R"({"ok":true})");
}

extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_free_string(const char* value) {
    std::free(const_cast<char*>(value));
}

// Game Hooks

extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_game_attach_surface(void* native_window, int width, int height) {
    gRenderer.Start(static_cast<ANativeWindow*>(native_window), width, height);
}

extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_game_resize_surface(int width, int height) {
    // Usually handled by restarting or adjusting viewport
}

extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_game_detach_surface() {
    gRenderer.Stop();
}

extern "C" INSTALINK_EXTENSION_EXPORT const char* instalink_extension_game_handle_input(const char* payload_json) {
    std::string json = payload_json ? payload_json : "";
    
    // Very naive JSON parsing for demonstration
    if (json.find("\"pointer_down\"") != std::string::npos || json.find("\"down\"") != std::string::npos) {
        size_t xPos = json.find("\"x\":");
        size_t yPos = json.find("\"y\":");
        if (xPos != std::string::npos && yPos != std::string::npos) {
            double x = std::atof(json.c_str() + xPos + 4);
            double y = std::atof(json.c_str() + yPos + 4);
            gRenderer.HandleInput(x, y);
        }
    }
    
    return DupString(R"({"ok":true})");
}


