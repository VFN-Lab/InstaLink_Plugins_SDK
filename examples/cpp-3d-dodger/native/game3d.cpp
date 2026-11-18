#include <android/log.h>
#include <android/native_window.h>
#include "instalink_extension_api.h"

#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>
#include <random>
#include <cmath>

#define LOG_TAG "Dodger3D"
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

struct Point3D {
    float x, y, z;
};

struct Point2D {
    int x, y;
};

struct Box {
    Point3D center;
    float size;
};

class Game3DRenderer {
public:
    void Start(ANativeWindow* window, int width, int height) {
        std::lock_guard<std::mutex> lock(mtx);
        this->window = window;
        this->screenWidth = width;
        this->screenHeight = height;
        
        if (window) {
            ANativeWindow_acquire(window);
            ANativeWindow_setBuffersGeometry(window, width, height, WINDOW_FORMAT_RGBA_8888);
            
            InitGame();
            
            running = true;
            renderThread = std::thread([this]() { Loop(); });
            LOGI("3D Game started: %dx%d", width, height);
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
    }

    void MovePlayer(float dx, float dy) {
        std::lock_guard<std::mutex> lock(mtx);
        if (gameOver) {
            if (gameOverTimer > 60) InitGame(); // allow reset after 1 sec
            return;
        }
        playerX += dx;
        playerY += dy;
        
        // Bounds
        if (playerX < -15.0f) playerX = -15.0f;
        if (playerX > 15.0f) playerX = 15.0f;
        if (playerY < -15.0f) playerY = -15.0f;
        if (playerY > 15.0f) playerY = 15.0f;
    }

private:
    void InitGame() {
        playerX = 0.0f;
        playerY = 0.0f;
        boxes.clear();
        speed = 0.5f;
        score = 0;
        gameOver = false;
        gameOverTimer = 0;
        
        for (int i = 0; i < 8; ++i) {
            SpawnBox(30.0f + i * 20.0f);
        }
    }

    void SpawnBox(float z) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(-12.0f, 12.0f);
        
        Box b;
        b.center = {dist(gen), dist(gen), z};
        b.size = 3.0f;
        boxes.push_back(b);
    }

    void Loop() {
        while (running) {
            UpdateState();
            DrawFrame();
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }
    }

    void UpdateState() {
        std::lock_guard<std::mutex> lock(mtx);
        if (gameOver) {
            gameOverTimer++;
            return;
        }

        speed += 0.0001f; // Slowly increase difficulty

        for (auto& b : boxes) {
            b.center.z -= speed;
            
            // Collision detection
            if (b.center.z > 0.0f && b.center.z < 2.0f) {
                if (std::abs(playerX - b.center.x) < b.size && 
                    std::abs(playerY - b.center.y) < b.size) {
                    gameOver = true;
                    return;
                }
            }
            
            if (b.center.z < 0.0f) {
                // Reset box far away
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_real_distribution<float> dist(-15.0f, 15.0f);
                b.center.x = dist(gen);
                b.center.y = dist(gen);
                b.center.z = 100.0f; // Max distance
                score++;
            }
        }
    }

    Point2D Project(Point3D p) {
        float fov = 300.0f;
        if (p.z < 0.1f) p.z = 0.1f;
        int sx = static_cast<int>(screenWidth / 2.0f + (p.x / p.z) * fov);
        int sy = static_cast<int>(screenHeight / 2.0f - (p.y / p.z) * fov); // Y inverted for screen
        return {sx, sy};
    }

    void DrawLine(uint32_t* pixels, int stride, int x0, int y0, int x1, int y1, uint32_t color) {
        // Bresenham's Line Algorithm
        int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy, e2;

        while (true) {
            if (x0 >= 0 && x0 < screenWidth && y0 >= 0 && y0 < screenHeight) {
                pixels[y0 * stride + x0] = color;
            }
            if (x0 == x1 && y0 == y1) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    void DrawBoxWireframe(uint32_t* pixels, int stride, Box b, uint32_t color) {
        if (b.center.z < 1.0f) return;

        float s = b.size;
        Point3D corners[8] = {
            {b.center.x - s, b.center.y - s, b.center.z - s},
            {b.center.x + s, b.center.y - s, b.center.z - s},
            {b.center.x + s, b.center.y + s, b.center.z - s},
            {b.center.x - s, b.center.y + s, b.center.z - s},
            {b.center.x - s, b.center.y - s, b.center.z + s},
            {b.center.x + s, b.center.y - s, b.center.z + s},
            {b.center.x + s, b.center.y + s, b.center.z + s},
            {b.center.x - s, b.center.y + s, b.center.z + s}
        };

        Point2D proj[8];
        for (int i = 0; i < 8; ++i) {
            proj[i] = Project(corners[i]);
        }

        // Draw Front face
        DrawLine(pixels, stride, proj[0].x, proj[0].y, proj[1].x, proj[1].y, color);
        DrawLine(pixels, stride, proj[1].x, proj[1].y, proj[2].x, proj[2].y, color);
        DrawLine(pixels, stride, proj[2].x, proj[2].y, proj[3].x, proj[3].y, color);
        DrawLine(pixels, stride, proj[3].x, proj[3].y, proj[0].x, proj[0].y, color);

        // Draw Back face
        DrawLine(pixels, stride, proj[4].x, proj[4].y, proj[5].x, proj[5].y, color);
        DrawLine(pixels, stride, proj[5].x, proj[5].y, proj[6].x, proj[6].y, color);
        DrawLine(pixels, stride, proj[6].x, proj[6].y, proj[7].x, proj[7].y, color);
        DrawLine(pixels, stride, proj[7].x, proj[7].y, proj[4].x, proj[4].y, color);

        // Connect faces
        DrawLine(pixels, stride, proj[0].x, proj[0].y, proj[4].x, proj[4].y, color);
        DrawLine(pixels, stride, proj[1].x, proj[1].y, proj[5].x, proj[5].y, color);
        DrawLine(pixels, stride, proj[2].x, proj[2].y, proj[6].x, proj[6].y, color);
        DrawLine(pixels, stride, proj[3].x, proj[3].y, proj[7].x, proj[7].y, color);
    }

    void DrawFrame() {
        std::lock_guard<std::mutex> lock(mtx);
        if (!window) return;

        ANativeWindow_Buffer buffer;
        if (ANativeWindow_lock(window, &buffer, nullptr) != 0) {
            return;
        }

        uint32_t* pixels = static_cast<uint32_t*>(buffer.bits);
        int stride = buffer.stride;
        
        // Background color (very dark purple)
        uint32_t bgColor = PackColor(10, 5, 20);
        if (gameOver) bgColor = PackColor(50, 0, 0); // Red flash

        for (int y = 0; y < screenHeight; ++y) {
            for (int x = 0; x < screenWidth; ++x) {
                pixels[y * stride + x] = bgColor;
            }
        }

        // Draw Horizon / Tunnel lines
        uint32_t gridColor = PackColor(30, 20, 50);
        Point2D center = Project({0, 0, 100.0f});
        DrawLine(pixels, stride, 0, 0, center.x, center.y, gridColor);
        DrawLine(pixels, stride, screenWidth, 0, center.x, center.y, gridColor);
        DrawLine(pixels, stride, 0, screenHeight, center.x, center.y, gridColor);
        DrawLine(pixels, stride, screenWidth, screenHeight, center.x, center.y, gridColor);

        // Sort boxes by Z (painter's algorithm)
        std::vector<Box> sorted = boxes;
        std::sort(sorted.begin(), sorted.end(), [](const Box& a, const Box& b) {
            return a.center.z > b.center.z;
        });

        // Draw boxes
        uint32_t boxColor = PackColor(0, 255, 255); // Neon Cyan
        for (const auto& b : sorted) {
            // Fade color based on distance
            float intensity = std::max(0.1f, 1.0f - (b.center.z / 100.0f));
            uint32_t c = PackColor(0, static_cast<uint8_t>(255 * intensity), static_cast<uint8_t>(255 * intensity));
            DrawBoxWireframe(pixels, stride, b, c);
        }

        // Draw Player (Ship/Crosshair at fixed Z near camera)
        Point2D p1 = Project({playerX, playerY + 1.0f, 3.0f});
        Point2D p2 = Project({playerX - 1.0f, playerY - 1.0f, 3.0f});
        Point2D p3 = Project({playerX + 1.0f, playerY - 1.0f, 3.0f});

        uint32_t playerColor = PackColor(255, 255, 0); // Yellow ship
        DrawLine(pixels, stride, p1.x, p1.y, p2.x, p2.y, playerColor);
        DrawLine(pixels, stride, p2.x, p2.y, p3.x, p3.y, playerColor);
        DrawLine(pixels, stride, p3.x, p3.y, p1.x, p1.y, playerColor);

        ANativeWindow_unlockAndPost(window);
    }

    std::mutex mtx;
    ANativeWindow* window = nullptr;
    int screenWidth = 0, screenHeight = 0;
    std::thread renderThread;
    std::atomic<bool> running{false};

    float playerX = 0.0f;
    float playerY = 0.0f;
    std::vector<Box> boxes;
    float speed = 0.5f;
    int score = 0;
    bool gameOver = false;
    int gameOverTimer = 0;
};

Game3DRenderer gGame;

} // namespace

extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_on_load(const char*) {}
extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_on_unload() {}

extern "C" INSTALINK_EXTENSION_EXPORT const char* instalink_extension_describe() {
    return DupString(R"({"ok":true,"adapter":"dodger3d","immersive":true})");
}

extern "C" INSTALINK_EXTENSION_EXPORT const char* instalink_extension_invoke(const char*, const char*) {
    return DupString(R"({"ok":true})");
}

extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_free_string(const char* value) {
    std::free(const_cast<char*>(value));
}

// Game Hooks

extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_game_attach_surface(void* native_window, int width, int height) {
    gGame.Start(static_cast<ANativeWindow*>(native_window), width, height);
}

extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_game_resize_surface(int, int) {
    // Ignore, let Android handle scaling or adapt viewport if needed
}

extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_game_detach_surface() {
    gGame.Stop();
}

extern "C" INSTALINK_EXTENSION_EXPORT const char* instalink_extension_game_handle_input(const char* payload_json) {
    std::string json = payload_json ? payload_json : "";
    
    float moveSpeed = 3.0f;

    if (json.find("\"action\":\"press\"") != std::string::npos || json.find("\"action\": \"press\"") != std::string::npos) {
        if (json.find("\"button\":\"up\"") != std::string::npos || json.find("\"button\": \"up\"") != std::string::npos) {
            gGame.MovePlayer(0.0f, moveSpeed);
        } else if (json.find("\"button\":\"down\"") != std::string::npos || json.find("\"button\": \"down\"") != std::string::npos) {
            gGame.MovePlayer(0.0f, -moveSpeed);
        } else if (json.find("\"button\":\"left\"") != std::string::npos || json.find("\"button\": \"left\"") != std::string::npos) {
            gGame.MovePlayer(-moveSpeed, 0.0f);
        } else if (json.find("\"button\":\"right\"") != std::string::npos || json.find("\"button\": \"right\"") != std::string::npos) {
            gGame.MovePlayer(moveSpeed, 0.0f);
        }
    }
    
    return DupString(R"({"ok":true})");
}


