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

#define LOG_TAG "SnakeGame"
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

struct Point {
    int x, y;
    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
};

enum Direction { UP, DOWN, LEFT, RIGHT };

class SnakeGameRenderer {
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
            LOGI("Snake Game started: %dx%d", width, height);
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

    void SetDirection(Direction newDir) {
        std::lock_guard<std::mutex> lock(mtx);
        if (gameOver) return;
        
        // Prevent 180 degree turns
        if (dir == UP && newDir == DOWN) return;
        if (dir == DOWN && newDir == UP) return;
        if (dir == LEFT && newDir == RIGHT) return;
        if (dir == RIGHT && newDir == LEFT) return;
        
        nextDir = newDir;
    }

private:
    void InitGame() {
        snake.clear();
        snake.push_back({GRID_SIZE / 2, GRID_SIZE / 2});
        snake.push_back({GRID_SIZE / 2, GRID_SIZE / 2 + 1});
        snake.push_back({GRID_SIZE / 2, GRID_SIZE / 2 + 2});
        dir = UP;
        nextDir = UP;
        gameOver = false;
        SpawnFood();
    }

    void SpawnFood() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, GRID_SIZE - 1);
        
        while (true) {
            food.x = dist(gen);
            food.y = dist(gen);
            bool collision = false;
            for (const auto& segment : snake) {
                if (segment == food) { collision = true; break; }
            }
            if (!collision) break;
        }
    }

    void Loop() {
        auto lastUpdate = std::chrono::steady_clock::now();
        const auto updateInterval = std::chrono::milliseconds(150);

        while (running) {
            auto now = std::chrono::steady_clock::now();
            if (now - lastUpdate >= updateInterval) {
                UpdateState();
                lastUpdate = now;
            }
            DrawFrame();
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // Render ~60FPS
        }
    }

    void UpdateState() {
        std::lock_guard<std::mutex> lock(mtx);
        if (gameOver) return;

        dir = nextDir;
        Point head = snake.front();
        Point newHead = head;

        switch (dir) {
            case UP:    newHead.y -= 1; break;
            case DOWN:  newHead.y += 1; break;
            case LEFT:  newHead.x -= 1; break;
            case RIGHT: newHead.x += 1; break;
        }

        // Wall collision
        if (newHead.x < 0 || newHead.x >= GRID_SIZE || newHead.y < 0 || newHead.y >= GRID_SIZE) {
            gameOver = true;
            return;
        }

        // Self collision
        for (const auto& segment : snake) {
            if (newHead == segment) {
                gameOver = true;
                return;
            }
        }

        snake.insert(snake.begin(), newHead);

        if (newHead == food) {
            SpawnFood();
        } else {
            snake.pop_back();
        }
    }

    void DrawRect(uint32_t* pixels, int stride, int px, int py, int w, int h, uint32_t color) {
        for (int y = py; y < py + h; ++y) {
            if (y < 0 || y >= screenHeight) continue;
            for (int x = px; x < px + w; ++x) {
                if (x < 0 || x >= screenWidth) continue;
                pixels[y * stride + x] = color;
            }
        }
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
        
        // Dark background
        uint32_t bgColor = PackColor(18, 18, 18);
        for (int y = 0; y < screenHeight; ++y) {
            for (int x = 0; x < screenWidth; ++x) {
                pixels[y * stride + x] = bgColor;
            }
        }

        // Calculate grid drawing dimensions
        int cellSize = std::min(screenWidth, screenHeight) / GRID_SIZE;
        int offsetX = (screenWidth - (cellSize * GRID_SIZE)) / 2;
        int offsetY = (screenHeight - (cellSize * GRID_SIZE)) / 2;

        // Draw Play Area Border
        DrawRect(pixels, stride, offsetX - 2, offsetY - 2, cellSize * GRID_SIZE + 4, cellSize * GRID_SIZE + 4, PackColor(100, 100, 100));
        DrawRect(pixels, stride, offsetX, offsetY, cellSize * GRID_SIZE, cellSize * GRID_SIZE, PackColor(30, 30, 30));

        if (gameOver) {
            // Fill red tint if game over
            for (int y = offsetY; y < offsetY + cellSize * GRID_SIZE; ++y) {
                for (int x = offsetX; x < offsetX + cellSize * GRID_SIZE; ++x) {
                    pixels[y * stride + x] = PackColor(100, 20, 20);
                }
            }
        }

        // Draw Food
        uint32_t foodColor = PackColor(255, 80, 80);
        DrawRect(pixels, stride, offsetX + food.x * cellSize + 1, offsetY + food.y * cellSize + 1, cellSize - 2, cellSize - 2, foodColor);

        // Draw Snake
        uint32_t snakeHeadColor = PackColor(120, 255, 120);
        uint32_t snakeBodyColor = PackColor(80, 200, 80);
        
        for (size_t i = 0; i < snake.size(); ++i) {
            uint32_t color = (i == 0) ? snakeHeadColor : snakeBodyColor;
            DrawRect(pixels, stride, offsetX + snake[i].x * cellSize + 1, offsetY + snake[i].y * cellSize + 1, cellSize - 2, cellSize - 2, color);
        }

        ANativeWindow_unlockAndPost(window);
    }

    std::mutex mtx;
    ANativeWindow* window = nullptr;
    int screenWidth = 0, screenHeight = 0;
    std::thread renderThread;
    std::atomic<bool> running{false};

    const int GRID_SIZE = 20;
    std::vector<Point> snake;
    Point food;
    Direction dir;
    Direction nextDir;
    bool gameOver = false;
};

SnakeGameRenderer gGame;

} // namespace

extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_on_load(const char*) {}
extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_on_unload() {}

extern "C" INSTALINK_EXTENSION_EXPORT const char* instalink_extension_describe() {
    return DupString(R"({"ok":true,"adapter":"snake_game","immersive":true})");
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

extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_game_resize_surface(int width, int height) {
    // Just ignore resize or update variables
}

extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_game_detach_surface() {
    gGame.Stop();
}

extern "C" INSTALINK_EXTENSION_EXPORT const char* instalink_extension_game_handle_input(const char* payload_json) {
    std::string json = payload_json ? payload_json : "";
    
    // Very naive JSON parsing to catch D-Pad presses
    // e.g., {"type":"virtual_button","action":"press","button":"up"}
    if (json.find("\"action\":\"press\"") != std::string::npos || json.find("\"action\": \"press\"") != std::string::npos) {
        if (json.find("\"button\":\"up\"") != std::string::npos || json.find("\"button\": \"up\"") != std::string::npos) {
            gGame.SetDirection(UP);
        } else if (json.find("\"button\":\"down\"") != std::string::npos || json.find("\"button\": \"down\"") != std::string::npos) {
            gGame.SetDirection(DOWN);
        } else if (json.find("\"button\":\"left\"") != std::string::npos || json.find("\"button\": \"left\"") != std::string::npos) {
            gGame.SetDirection(LEFT);
        } else if (json.find("\"button\":\"right\"") != std::string::npos || json.find("\"button\": \"right\"") != std::string::npos) {
            gGame.SetDirection(RIGHT);
        }
    }
    
    return DupString(R"({"ok":true})");
}


