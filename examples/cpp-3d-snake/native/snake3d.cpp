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
#include <deque>

#define LOG_TAG "Snake3D"
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
    uint32_t color;
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
            LOGI("3D Snake started: %dx%d", width, height);
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
        
        // Disallow moving directly backwards
        if (dx != 0 && dirX == -dx) return;
        if (dy != 0 && dirY == -dy) return;
        
        // Prevent diagonal movement in grid
        if (dx != 0) {
            dirX = dx;
            dirY = 0;
        } else if (dy != 0) {
            dirX = 0;
            dirY = dy;
        }
    }

private:
    void InitGame() {
        snake.clear();
        dirX = 1.0f; // Start moving right
        dirY = 0.0f;
        
        // Add initial 3 segments
        snake.push_front({0.0f, 0.0f, 10.0f});
        snake.push_back({-2.0f, 0.0f, 10.0f});
        snake.push_back({-4.0f, 0.0f, 10.0f});
        
        score = 0;
        gameOver = false;
        gameOverTimer = 0;
        moveCounter = 0;
        moveDelay = 15; // Frames between moves (speed)
        
        SpawnFood();
    }

    void SpawnFood() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(-5, 5); // Grid limits
        
        food.x = dist(gen) * 2.0f;
        food.y = dist(gen) * 2.0f;
        food.z = 10.0f; // Keep food at constant Z relative to camera
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

        moveCounter++;
        if (moveCounter >= moveDelay) {
            moveCounter = 0;
            
            // Move snake head
            Point3D head = snake.front();
            Point3D newHead = {head.x + dirX * 2.0f, head.y + dirY * 2.0f, head.z};
            
            // Wall collision (Grid from -12 to 12)
            if (newHead.x < -12.0f || newHead.x > 12.0f || newHead.y < -12.0f || newHead.y > 12.0f) {
                gameOver = true;
                return;
            }
            
            // Self collision
            for (const auto& s : snake) {
                if (std::abs(newHead.x - s.x) < 0.1f && std::abs(newHead.y - s.y) < 0.1f) {
                    gameOver = true;
                    return;
                }
            }
            
            snake.push_front(newHead);
            
            // Food collision
            if (std::abs(newHead.x - food.x) < 0.1f && std::abs(newHead.y - food.y) < 0.1f) {
                score++;
                if (moveDelay > 5) moveDelay--; // Increase speed slightly
                SpawnFood();
            } else {
                snake.pop_back(); // Remove tail if no food eaten
            }
        }
    }

    Point2D Project(Point3D p, Point3D camera) {
        float fov = 400.0f;
        float dz = p.z - camera.z;
        if (dz < 0.1f) dz = 0.1f;
        int sx = static_cast<int>(screenWidth / 2.0f + ((p.x - camera.x) / dz) * fov);
        int sy = static_cast<int>(screenHeight / 2.0f - ((p.y - camera.y) / dz) * fov); // Y inverted for screen
        return {sx, sy};
    }

    void DrawLine(uint32_t* pixels, int stride, int x0, int y0, int x1, int y1, uint32_t color) {
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

    void DrawBoxWireframe(uint32_t* pixels, int stride, Box b, Point3D camera) {
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
            proj[i] = Project(corners[i], camera);
        }

        // Draw Front face
        DrawLine(pixels, stride, proj[0].x, proj[0].y, proj[1].x, proj[1].y, b.color);
        DrawLine(pixels, stride, proj[1].x, proj[1].y, proj[2].x, proj[2].y, b.color);
        DrawLine(pixels, stride, proj[2].x, proj[2].y, proj[3].x, proj[3].y, b.color);
        DrawLine(pixels, stride, proj[3].x, proj[3].y, proj[0].x, proj[0].y, b.color);

        // Connect faces (Only draw depth lines to make it look 3D but less cluttered)
        DrawLine(pixels, stride, proj[0].x, proj[0].y, proj[4].x, proj[4].y, b.color);
        DrawLine(pixels, stride, proj[1].x, proj[1].y, proj[5].x, proj[5].y, b.color);
        DrawLine(pixels, stride, proj[2].x, proj[2].y, proj[6].x, proj[6].y, b.color);
        DrawLine(pixels, stride, proj[3].x, proj[3].y, proj[7].x, proj[7].y, b.color);
        
        // Draw Back face
        DrawLine(pixels, stride, proj[4].x, proj[4].y, proj[5].x, proj[5].y, b.color);
        DrawLine(pixels, stride, proj[5].x, proj[5].y, proj[6].x, proj[6].y, b.color);
        DrawLine(pixels, stride, proj[6].x, proj[6].y, proj[7].x, proj[7].y, b.color);
        DrawLine(pixels, stride, proj[7].x, proj[7].y, proj[4].x, proj[4].y, b.color);
    }

    void DrawFrame() {
        std::lock_guard<std::mutex> lock(mtx);
        if (!window || snake.empty()) return;

        ANativeWindow_Buffer buffer;
        if (ANativeWindow_lock(window, &buffer, nullptr) != 0) {
            return;
        }

        uint32_t* pixels = static_cast<uint32_t*>(buffer.bits);
        int stride = buffer.stride;
        
        // Background color
        uint32_t bgColor = PackColor(10, 10, 20); // Dark Blueish
        if (gameOver) bgColor = PackColor(50, 0, 0); // Red flash

        for (int y = 0; y < screenHeight; ++y) {
            for (int x = 0; x < screenWidth; ++x) {
                pixels[y * stride + x] = bgColor;
            }
        }

        // Camera setup (Third person slightly behind and below)
        Point3D head = snake.front();
        Point3D camera = {head.x, head.y - 4.0f, head.z - 20.0f};

        // Draw Grid lines
        uint32_t gridColor = PackColor(20, 30, 40);
        for(float i = -14.0f; i <= 14.0f; i+=2.0f) {
            Point2D startX = Project({i, -14.0f, 10.0f}, camera);
            Point2D endX = Project({i, 14.0f, 10.0f}, camera);
            DrawLine(pixels, stride, startX.x, startX.y, endX.x, endX.y, gridColor);
            
            Point2D startY = Project({-14.0f, i, 10.0f}, camera);
            Point2D endY = Project({14.0f, i, 10.0f}, camera);
            DrawLine(pixels, stride, startY.x, startY.y, endY.x, endY.y, gridColor);
        }

        // Draw Food
        Box foodBox = {food, 0.8f, PackColor(255, 0, 0)}; // Red apple
        DrawBoxWireframe(pixels, stride, foodBox, camera);

        // Draw Snake (Painter's algorithm: draw from tail to head)
        for (int i = snake.size() - 1; i >= 0; --i) {
            float c = 1.0f - ((float)i / (snake.size() + 1)); // Head is brightest
            uint32_t sColor = PackColor(0, static_cast<uint8_t>(255 * c), static_cast<uint8_t>(100 * c));
            if (i == 0) sColor = PackColor(0, 255, 255); // Head is Cyan
            
            Box sb = {snake[i], 0.9f, sColor};
            DrawBoxWireframe(pixels, stride, sb, camera);
        }

        ANativeWindow_unlockAndPost(window);
    }

    std::mutex mtx;
    ANativeWindow* window = nullptr;
    int screenWidth = 0, screenHeight = 0;
    std::thread renderThread;
    std::atomic<bool> running{false};

    std::deque<Point3D> snake;
    Point3D food;
    float dirX = 1.0f;
    float dirY = 0.0f;
    
    int moveCounter = 0;
    int moveDelay = 15;
    
    int score = 0;
    bool gameOver = false;
    int gameOverTimer = 0;
};

Game3DRenderer gGame;

} // namespace

extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_on_load(const char*) {}
extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_on_unload() {}

extern "C" INSTALINK_EXTENSION_EXPORT const char* instalink_extension_describe() {
    return DupString(R"({"ok":true,"adapter":"snake3d","immersive":true})");
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
}

extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_game_detach_surface() {
    gGame.Stop();
}

extern "C" INSTALINK_EXTENSION_EXPORT const char* instalink_extension_game_handle_input(const char* payload_json) {
    std::string json = payload_json ? payload_json : "";
    
    if (json.find("\"action\":\"press\"") != std::string::npos || json.find("\"action\": \"press\"") != std::string::npos) {
        if (json.find("\"button\":\"up\"") != std::string::npos || json.find("\"button\": \"up\"") != std::string::npos) {
            gGame.MovePlayer(0.0f, 1.0f);
        } else if (json.find("\"button\":\"down\"") != std::string::npos || json.find("\"button\": \"down\"") != std::string::npos) {
            gGame.MovePlayer(0.0f, -1.0f);
        } else if (json.find("\"button\":\"left\"") != std::string::npos || json.find("\"button\": \"left\"") != std::string::npos) {
            gGame.MovePlayer(-1.0f, 0.0f);
        } else if (json.find("\"button\":\"right\"") != std::string::npos || json.find("\"button\": \"right\"") != std::string::npos) {
            gGame.MovePlayer(1.0f, 0.0f);
        }
    }
    
    return DupString(R"({"ok":true})");
}
