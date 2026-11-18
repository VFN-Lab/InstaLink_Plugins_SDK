/**
 * InstaLink Extension API — Stable C ABI
 * 
 * Include this header in your native extension adapter and implement
 * the required functions. The InstaLink browser host will load your
 * compiled .so via dlopen/dlsym and call these functions.
 *
 * Required functions:
 *   instalink_extension_describe()      — Return a JSON string describing your adapter.
 *   instalink_extension_invoke()        — Handle an action with a JSON payload.
 *   instalink_extension_free_string()   — Free a string previously returned by your adapter.
 *
 * Optional lifecycle hooks:
 *   instalink_extension_on_load()       — Called when the extension is loaded (receives runtime root).
 *   instalink_extension_on_unload()     — Called when the extension is unloaded.
 *
 * Optional immersive game hooks:
 *   instalink_extension_game_attach_surface()   — Receive an ANativeWindow for rendering.
 *   instalink_extension_game_resize_surface()   — Handle surface size changes.
 *   instalink_extension_game_detach_surface()   — Release the surface.
 *   instalink_extension_game_handle_input()     — Handle touch/button input.
 *
 * Memory contract:
 *   - Strings returned by describe(), invoke(), and game_handle_input() must
 *     be allocated with malloc(). The host will call free_string() to release them.
 *   - All returned strings must be valid JSON.
 *
 * @version 2.0
 * @see https://github.com/VFN-Lab/InstaLink_Plugins_SDK
 */

#pragma once

#if defined(_WIN32)
#define INSTALINK_EXTENSION_EXPORT __declspec(dllexport)
#else
#define INSTALINK_EXTENSION_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Lifecycle (optional) ──────────────────────────────────────────── */

/**
 * Called once when the host loads the extension adapter.
 * @param runtime_root  Absolute path to the extracted extension directory.
 *                      Use this to resolve model files, config, etc.
 */
INSTALINK_EXTENSION_EXPORT void instalink_extension_on_load(const char* runtime_root);

/**
 * Called once when the host unloads the extension adapter.
 * Use this to release resources, close files, etc.
 */
INSTALINK_EXTENSION_EXPORT void instalink_extension_on_unload();

/* ─── Core ABI (required) ──────────────────────────────────────────── */

/**
 * Return a JSON string describing this adapter.
 * Example: {"ok":true,"adapter":"my_adapter","actions":["echo","process"]}
 *
 * @return  A malloc'd JSON string. The host will call free_string() on it.
 */
INSTALINK_EXTENSION_EXPORT const char* instalink_extension_describe();

/**
 * Handle an action invoked by the extension's JS UI or the host.
 *
 * @param action        The action name (e.g., "echo", "synthesize", "init").
 * @param payload_json  A JSON string with action parameters.
 * @return              A malloc'd JSON result string. Must include "ok":true/false.
 */
INSTALINK_EXTENSION_EXPORT const char* instalink_extension_invoke(
    const char* action,
    const char* payload_json
);

/**
 * Free a string previously returned by describe(), invoke(), or game_handle_input().
 * @param value  The string to free. May be NULL (should be a no-op).
 */
INSTALINK_EXTENSION_EXPORT void instalink_extension_free_string(const char* value);

/* ─── Immersive Game Hooks (optional) ──────────────────────────────── */

/**
 * Called when the host provides a native rendering surface.
 * Use ANativeWindow APIs to draw directly to the surface.
 *
 * @param native_window  An ANativeWindow* pointer.
 * @param width          Initial surface width in pixels.
 * @param height         Initial surface height in pixels.
 */
INSTALINK_EXTENSION_EXPORT void instalink_extension_game_attach_surface(
    void* native_window,
    int width,
    int height
);

/**
 * Called when the surface dimensions change.
 * @param width   New width in pixels.
 * @param height  New height in pixels.
 */
INSTALINK_EXTENSION_EXPORT void instalink_extension_game_resize_surface(
    int width,
    int height
);

/**
 * Called when the surface is being destroyed. Release all references.
 */
INSTALINK_EXTENSION_EXPORT void instalink_extension_game_detach_surface();

/**
 * Handle input events forwarded by the host shell.
 *
 * Input payloads include:
 *   {"type":"pointer_down","x":120.5,"y":340.2}
 *   {"type":"pointer_up","x":120.5,"y":340.2}
 *   {"type":"virtual_button","action":"press","button":"up"}
 *   {"type":"virtual_button","action":"press","button":"restart"}
 *
 * @param payload_json  JSON string describing the input event.
 * @return              A malloc'd JSON result string.
 */
INSTALINK_EXTENSION_EXPORT const char* instalink_extension_game_handle_input(
    const char* payload_json
);

#ifdef __cplusplus
}
#endif


