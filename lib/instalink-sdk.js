// InstaLink Browser Extension SDK - JavaScript Wrapper
// Clean API for interacting with the InstaLink Android host.
// Include in HTML: <script src="instalink-sdk.js"></script>
// Usage:
//   InstaLinkSDK.init("my_extension_id");
//   InstaLinkSDK.capabilities.tts.speak("Hello!");
// When running outside the InstaLink browser (e.g. desktop Chrome), APIs are mocked automatically.
// Version: 1.0.0 | License: MIT
// Repository: https://github.com/VFN-Lab/InstaLink_Plugins_SDK

(function (global) {
    "use strict";

    if (global.InstaLinkSDK) {
        return; // Already loaded
    }

    var InstaLinkSDK = {

        // The extension ID set during init()
        id: "",

        // True when running outside the InstaLink browser
        isMocked: false,

        // Initialize the SDK with the extension ID defined in manifest.json
        init: function (extensionId) {
            this.id = extensionId;
            if (!global.window.InstaLink) {
                console.warn("[InstaLinkSDK] InstaLink environment not detected. Mocking APIs for local testing.");
                this._mockEnvironment();
                this.isMocked = true;
            }
            console.log("[InstaLinkSDK] Initialized for extension: " + extensionId);
        },

        // Permissions

        permissions: {
            // Request a capability permission from browser settings & manifest
            request: function (permissionName) {
                return global.window.InstaLink.extension.requestPermission(
                    InstaLinkSDK.id,
                    permissionName
                );
            },

            // Inspect Android OS runtime permission status for a declared permission
            systemStatus: function (permissionName) {
                return global.window.InstaLink.extension.systemPermissionStatus(
                    InstaLinkSDK.id,
                    permissionName
                );
            },

            // Trigger the Android OS runtime permission prompt if missing
            requestSystem: function (permissionName) {
                return global.window.InstaLink.extension.requestSystemPermission(
                    InstaLinkSDK.id,
                    permissionName
                );
            }
        },

        // Capabilities (System, Media, Hardware)

        capabilities: {
            // List all capabilities available to this extension
            list: function () {
                return global.window.InstaLink.capabilities.list(InstaLinkSDK.id);
            },

            // Text-to-Speech (TTS)
            tts: {
                // Check or warm up TTS engine readiness
                status: function () {
                    return global.window.InstaLink.capabilities.tts.status(InstaLinkSDK.id);
                },

                // Speak the given text (language is a BCP 47 tag, default "en-US")
                speak: function (text, language) {
                    return global.window.InstaLink.capabilities.tts.speak(
                        InstaLinkSDK.id,
                        { text: text, language: language || "en-US" }
                    );
                },

                // Stop any active TTS playback
                stop: function () {
                    return global.window.InstaLink.capabilities.tts.stop(InstaLinkSDK.id);
                }
            },

            // System Notifications
            notifications: {
                // Show a system notification with title, body, and optional numeric id
                show: function (title, body, id) {
                    var payload = { title: title, body: body };
                    if (id !== undefined) payload.id = id;
                    return global.window.InstaLink.capabilities.notifications.show(
                        InstaLinkSDK.id,
                        payload
                    );
                }
            },

            // Audio Routing & Volume
            audio: {
                // Get current audio route ("speaker", "earpiece", "default")
                getRoute: function () {
                    return global.window.InstaLink.capabilities.audioRoute.get(InstaLinkSDK.id);
                },

                // Route audio output ("speaker", "earpiece", or "default")
                setRoute: function (route) {
                    return global.window.InstaLink.capabilities.audioRoute.set(
                        InstaLinkSDK.id,
                        route
                    );
                },

                // Get current & max media stream volume
                getVolume: function () {
                    return global.window.InstaLink.capabilities.volume.get(InstaLinkSDK.id);
                },

                // Set exact media volume level (clamped between 0 and max)
                setVolume: function (level) {
                    return global.window.InstaLink.capabilities.volume.set(
                        InstaLinkSDK.id,
                        level
                    );
                },

                // Step volume up (+1) or down (-1)
                adjustVolume: function (delta) {
                    return global.window.InstaLink.capabilities.volume.adjust(
                        InstaLinkSDK.id,
                        delta
                    );
                }
            },

            // Check vibrator support and amplitude control
            vibrationStatus: function () {
                return global.window.InstaLink.capabilities.invoke(
                    InstaLinkSDK.id,
                    "vibration",
                    "status",
                    {}
                );
            },

            // Trigger device vibration (20 to 5000 ms, default 180 ms)
            vibrate: function (durationMs) {
                return global.window.InstaLink.capabilities.invoke(
                    InstaLinkSDK.id,
                    "vibration",
                    "vibrate",
                    { duration_ms: durationMs || 180 }
                );
            },

            // Cancel ongoing vibration
            cancelVibrate: function () {
                return global.window.InstaLink.capabilities.invoke(
                    InstaLinkSDK.id,
                    "vibration",
                    "cancel",
                    {}
                );
            },

            // Show a brief toast message
            toast: function (message) {
                return global.window.InstaLink.browser.toast(message);
            },

            // Hardware Sensors
            sensors: {
                // List all physical sensors available on the device
                list: function () {
                    return global.window.InstaLink.capabilities.invoke(
                        InstaLinkSDK.id,
                        "sensors",
                        "list",
                        {}
                    );
                },

                // Capture a real-time snapshot of proximity, light, and accelerometer
                snapshot: function () {
                    return global.window.InstaLink.capabilities.invoke(
                        InstaLinkSDK.id,
                        "sensors",
                        "snapshot",
                        {}
                    );
                },

                // Return accelerometer coordinates { ok, x, y, z, values }
                getAccelerometer: function () {
                    var res = global.window.InstaLink.capabilities.invoke(
                        InstaLinkSDK.id,
                        "sensors",
                        "snapshot",
                        {}
                    );
                    var vals = (res && res.result && res.result.accelerometer && res.result.accelerometer.values) || [0, 0, 0];
                    return {
                        ok: !!(res && res.ok),
                        x: vals[0] || 0,
                        y: vals[1] || 0,
                        z: vals[2] || 0,
                        values: vals
                    };
                }
            },

            // Read hardware & Android OS info
            deviceInfo: function () {
                return global.window.InstaLink.capabilities.invoke(InstaLinkSDK.id, "device_info", "get", {});
            },

            // Read network & Wi-Fi state
            connectivityInfo: function () {
                return global.window.InstaLink.capabilities.invoke(InstaLinkSDK.id, "connectivity_info", "get", {});
            },

            // Read Bluetooth adapter, bonded & connected devices
            bluetoothInfo: function () {
                return global.window.InstaLink.capabilities.invoke(InstaLinkSDK.id, "bluetooth_info", "get", {});
            },

            // Resolve or launch installed Android apps by package_name
            apps: {
                resolve: function (packageName) {
                    return global.window.InstaLink.capabilities.invoke(
                        InstaLinkSDK.id,
                        "app_launch",
                        "resolve",
                        { package_name: packageName }
                    );
                },
                launch: function (packageName) {
                    return global.window.InstaLink.capabilities.invoke(
                        InstaLinkSDK.id,
                        "app_launch",
                        "launch",
                        { package_name: packageName }
                    );
                }
            },

            // Invoke any custom InstaLink Host capability registered by the host
            invokeAndroidHost: function (hostId, action, payload) {
                return global.window.InstaLink.capabilities.invoke(
                    InstaLinkSDK.id,
                    hostId,
                    action,
                    payload
                );
            }
        },

        // Persistent Storage

        storage: {
            // Store a value persistently
            set: function (key, value) {
                return global.window.InstaLink.storage.set(InstaLinkSDK.id, key, value);
            },

            // Retrieve a stored value or return defaultValue if not found
            get: function (key, defaultValue) {
                return global.window.InstaLink.storage.get(
                    InstaLinkSDK.id,
                    key,
                    defaultValue !== undefined ? defaultValue : null
                );
            }
        },

        // Native Runtime Bridge (for hybrid/native extensions)

        native: {
            // Check if the native runtime bridge is available
            isReady: function () {
                return !!(
                    global.window.InstaLink &&
                    global.window.InstaLink.nativeRuntime &&
                    global.window.InstaLink.__nativeReady
                );
            },

            // Inspect the loaded native adapter metadata
            inspect: function () {
                return global.window.InstaLink.nativeRuntime.inspect(InstaLinkSDK.id);
            },

            // Invoke a native adapter action with a plain payload object
            invoke: function (action, payload) {
                return global.window.InstaLink.nativeRuntime.invoke(
                    InstaLinkSDK.id,
                    action,
                    payload || {}
                );
            }
        },

        // Mock Environment (for local desktop testing)

        _mockEnvironment: function () {
            global.window.InstaLink = {
                __nativeReady: true,
                extension: {
                    requestPermission: function () { return true; },
                    systemPermissionStatus: function (id, perm) {
                        return { ok: true, permission: perm, android_permissions: [], granted_permissions: [], missing_permissions: [], requires_runtime_prompt: false };
                    },
                    requestSystemPermission: function (id, perm) {
                        return { ok: true, permission: perm, requested: false, already_granted: true };
                    }
                },
                browser: {
                    toast: function (msg) { console.log("[Mock Toast] " + msg); }
                },
                capabilities: {
                    list: function () {
                        return [
                            { name: "notifications", available: true },
                            { name: "speech_tts", available: true },
                            { name: "device_info", available: true },
                            { name: "connectivity_info", available: true },
                            { name: "bluetooth_info", available: true },
                            { name: "audio_route", available: true },
                            { name: "volume_control", available: true },
                            { name: "vibration", available: true },
                            { name: "sensors", available: true },
                            { name: "app_launch", available: true }
                        ];
                    },
                    tts: {
                        status: function () { return { ok: true, result: { ready: true, mocked: true } }; },
                        speak: function (id, p) { console.log("[Mock TTS] " + p.text); return { ok: true, mocked: true }; },
                        stop: function () { console.log("[Mock TTS] Stopped"); return { ok: true, mocked: true }; }
                    },
                    notifications: {
                        show: function (id, p) {
                            console.log("[Mock Notification] " + p.title + ": " + p.body);
                            return { ok: true, mocked: true, result: p };
                        }
                    },
                    audioRoute: {
                        get: function () { return { ok: true, mocked: true, result: { route: "default", mode: 0 } }; },
                        set: function (id, r) { console.log("[Mock Audio Route] " + r); return { ok: true, mocked: true, result: { route: r } }; }
                    },
                    volume: {
                        get: function () { return { ok: true, mocked: true, result: { stream: "music", value: 7, max: 15 } }; },
                        set: function (id, v) { console.log("[Mock Volume Set] " + v); return { ok: true, mocked: true, result: { stream: "music", value: v, max: 15 } }; },
                        adjust: function (id, v) { console.log("[Mock Volume Adjust] " + v); return { ok: true, mocked: true, result: { stream: "music", value: 7, max: 15 } }; }
                    },
                    invoke: function (id, cap, action, p) {
                        console.log("[Mock Capability] " + cap + "." + action, p);
                        if (cap === "sensors" && action === "snapshot") {
                            return {
                                ok: true,
                                mocked: true,
                                result: {
                                    proximity: { available: true, values: [5.0] },
                                    light: { available: true, values: [120.0] },
                                    accelerometer: { available: true, values: [0.1, 9.8, 0.2] }
                                }
                            };
                        }
                        return { ok: true, mocked: true, capability: cap, action: action, result: {} };
                    }
                },
                storage: {
                    _data: {},
                    set: function (id, k, v) { this._data[k] = v; return true; },
                    get: function (id, k, def) {
                        return this._data[k] !== undefined ? this._data[k] : def;
                    }
                },
                nativeRuntime: {
                    inspect: function () { return { ok: true, mocked: true, adapter: "mock" }; },
                    invoke: function (id, action, payload) {
                        console.log("[Mock Native] " + action, payload);
                        return { ok: true, mocked: true, action: action };
                    }
                }
            };
        }
    };

    global.InstaLinkSDK = InstaLinkSDK;

})(window);
