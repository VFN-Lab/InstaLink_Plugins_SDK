#pragma once

#if defined(_WIN32)
#define INSTALINK_EXTENSION_EXPORT __declspec(dllexport)
#else
#define INSTALINK_EXTENSION_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

INSTALINK_EXTENSION_EXPORT void instalink_extension_on_load(const char* runtime_root);
INSTALINK_EXTENSION_EXPORT void instalink_extension_on_unload();
INSTALINK_EXTENSION_EXPORT const char* instalink_extension_describe();
INSTALINK_EXTENSION_EXPORT const char* instalink_extension_invoke(const char* action, const char* payload_json);
INSTALINK_EXTENSION_EXPORT void instalink_extension_free_string(const char* value);

INSTALINK_EXTENSION_EXPORT void instalink_extension_game_attach_surface(void* native_window, int width, int height);
INSTALINK_EXTENSION_EXPORT void instalink_extension_game_resize_surface(int width, int height);
INSTALINK_EXTENSION_EXPORT void instalink_extension_game_detach_surface();
INSTALINK_EXTENSION_EXPORT const char* instalink_extension_game_handle_input(const char* payload_json);

#ifdef __cplusplus
}
#endif
