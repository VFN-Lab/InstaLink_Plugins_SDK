#include <android/log.h>
#include <android/native_window.h>
#include "instalink_extension_api.h"

#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <sstream>

#define LOG_TAG "TicTacToe"

using namespace std;

namespace {

char* DupString(const string& value) {
    char* buffer = static_cast<char*>(malloc(value.size() + 1));
    if (!buffer) return nullptr;
    memcpy(buffer, value.c_str(), value.size() + 1);
    return buffer;
}

uint32_t PackColor(uint8_t r, uint8_t g, uint8_t b) {
    return (255 << 24) | (b << 16) | (g << 8) | r;
}

void FillRect(uint32_t* pixels, int stride, int left, int top, int right, int bottom, uint32_t color) {
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            pixels[y * stride + x] = color;
        }
    }
}

class TicTacToeGame {
public:
    void AttachSurface(ANativeWindow* window, int width, int height) {
        lock_guard<mutex> lock(mtx);
        this->window = window;
        this->width = width;
        this->height = height;
        if (window) {
            ANativeWindow_acquire(window);
            ANativeWindow_setBuffersGeometry(window, width, height, WINDOW_FORMAT_RGBA_8888);
            StartRender();
        }
    }

    void DetachSurface() {
        StopRender();
        lock_guard<mutex> lock(mtx);
        if (window) {
            ANativeWindow_release(window);
            window = nullptr;
        }
    }

    void HandleInput(double x, double y) {
        lock_guard<mutex> lock(mtx);
        if (gameOver) {
            ResetGame();
            return;
        }
        
        int cellSize = min(width, height) / 3;
        int offsetX = (width - cellSize * 3) / 2;
        int offsetY = (height - cellSize * 3) / 2;

        int col = (x - offsetX) / cellSize;
        int row = (y - offsetY) / cellSize;

        if (col >= 0 && col < 3 && row >= 0 && row < 3) {
            int idx = row * 3 + col;
            if (board[idx] == 0) {
                board[idx] = 1; // Player X
                if (!CheckWin()) {
                    AIMove();
                    CheckWin();
                }
            }
        }
    }

private:
    void StartRender() {
        running = true;
        renderThread = thread([this]() {
            while (running) {
                DrawFrame();
                this_thread::sleep_for(chrono::milliseconds(30));
            }
        });
    }

    void StopRender() {
        running = false;
        if (renderThread.joinable()) renderThread.join();
    }

    void ResetGame() {
        fill(board.begin(), board.end(), 0);
        gameOver = false;
    }

    bool CheckWin() {
        int wins[8][3] = {
            {0,1,2}, {3,4,5}, {6,7,8}, {0,3,6}, {1,4,7}, {2,5,8}, {0,4,8}, {2,4,6}
        };
        for (auto& w : wins) {
            if (board[w[0]] != 0 && board[w[0]] == board[w[1]] && board[w[1]] == board[w[2]]) {
                gameOver = true;
                return true;
            }
        }
        for (int c : board) {
            if (c == 0) return false;
        }
        gameOver = true; // Draw
        return true;
    }

    void AIMove() {
        for (int i = 0; i < 9; ++i) {
            if (board[i] == 0) {
                board[i] = 2; // AI O
                break;
            }
        }
    }

    void DrawFrame() {
        ANativeWindow* win = nullptr;
        {
            lock_guard<mutex> lock(mtx);
            win = window;
            if (!win) return;
            ANativeWindow_acquire(win);
        }

        ANativeWindow_Buffer buffer;
        if (ANativeWindow_lock(win, &buffer, nullptr) != 0) {
            ANativeWindow_release(win);
            return;
        }

        uint32_t* pixels = static_cast<uint32_t*>(buffer.bits);
        int stride = buffer.stride;
        
        // Background
        FillRect(pixels, stride, 0, 0, width, height, PackColor(15, 23, 42));

        int cellSize = min(width, height) / 3;
        int offsetX = (width - cellSize * 3) / 2;
        int offsetY = (height - cellSize * 3) / 2;

        // Draw Grid
        uint32_t gridColor = PackColor(56, 189, 248);
        for (int i = 1; i < 3; ++i) {
            FillRect(pixels, stride, offsetX + i * cellSize - 2, offsetY, offsetX + i * cellSize + 2, offsetY + cellSize * 3, gridColor);
            FillRect(pixels, stride, offsetX, offsetY + i * cellSize - 2, offsetX + cellSize * 3, offsetY + i * cellSize + 2, gridColor);
        }

        // Draw X and O
        for (int i = 0; i < 9; ++i) {
            if (board[i] == 0) continue;
            int r = i / 3;
            int c = i % 3;
            int cx = offsetX + c * cellSize + cellSize / 2;
            int cy = offsetY + r * cellSize + cellSize / 2;
            
            if (board[i] == 1) { // X
                FillRect(pixels, stride, cx - 20, cy - 20, cx + 20, cy + 20, PackColor(34, 197, 94));
            } else { // O
                FillRect(pixels, stride, cx - 20, cy - 20, cx + 20, cy + 20, PackColor(239, 68, 68));
            }
        }

        if (gameOver) {
            FillRect(pixels, stride, 0, height/2 - 40, width, height/2 + 40, PackColor(0,0,0));
        }

        ANativeWindow_unlockAndPost(win);
        ANativeWindow_release(win);
    }

    mutex mtx;
    ANativeWindow* window = nullptr;
    int width = 0, height = 0;
    thread renderThread;
    bool running = false;

    vector<int> board = vector<int>(9, 0);
    bool gameOver = false;
};

TicTacToeGame gGame;

} // namespace

extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_on_load(const char*) {}
extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_on_unload() {}

extern "C" INSTALINK_EXTENSION_EXPORT const char* instalink_extension_describe() {
    return DupString(R"({"ok":true,"adapter":"tic_tac_toe","immersive":true})");
}

extern "C" INSTALINK_EXTENSION_EXPORT const char* instalink_extension_invoke(const char* action, const char* payload) {
    return DupString(R"({"ok":true})");
}

extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_free_string(const char* value) {
    free(const_cast<char*>(value));
}

extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_game_attach_surface(void* native_window, int width, int height) {
    gGame.AttachSurface(static_cast<ANativeWindow*>(native_window), width, height);
}

extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_game_resize_surface(int width, int height) {
}

extern "C" INSTALINK_EXTENSION_EXPORT void instalink_extension_game_detach_surface() {
    gGame.DetachSurface();
}

extern "C" INSTALINK_EXTENSION_EXPORT const char* instalink_extension_game_handle_input(const char* payload_json) {
    // Basic JSON parsing to extract pointer_down x and y
    string json = payload_json ? payload_json : "";
    if (json.find("\"pointer_down\"") != string::npos || json.find("\"down\"") != string::npos) {
        size_t xPos = json.find("\"x\":");
        size_t yPos = json.find("\"y\":");
        if (xPos != string::npos && yPos != string::npos) {
            double x = atof(json.c_str() + xPos + 4);
            double y = atof(json.c_str() + yPos + 4);
            gGame.HandleInput(x, y);
        }
    }
    return DupString(R"({"ok":true})");
}
