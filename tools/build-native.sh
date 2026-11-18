#!/usr/bin/env bash
set -euo pipefail

echo "==================================================="
echo "  InstalinkSDK - Native Extension Builder (Linux/macOS)"
echo "==================================================="

if [ "$#" -lt 1 ]; then
    echo "Usage: ./build-native.sh <path_to_extension_folder> [Debug|Release]"
    echo "Example: ./build-native.sh ../examples/native-echo"
    exit 1
fi

EXT_DIR="$(cd "$1" && pwd)"
BUILD_TYPE="${2:-Release}"
MANIFEST="$EXT_DIR/manifest.json"
NATIVE_DIR="$EXT_DIR/native"

if [ ! -f "$MANIFEST" ]; then
    echo "Error: manifest.json not found in $EXT_DIR"
    exit 1
fi

if [ ! -f "$NATIVE_DIR/CMakeLists.txt" ]; then
    echo "Error: native/CMakeLists.txt not found in $EXT_DIR"
    exit 1
fi

SDK_ROOT="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$HOME/Android/Sdk}}"
NDK_ROOT="${ANDROID_NDK_HOME:-}"
if [ -z "$NDK_ROOT" ] && [ -d "$SDK_ROOT/ndk" ]; then
    NDK_ROOT="$(ls -1d "$SDK_ROOT/ndk/"* | sort -r | head -n 1)"
fi

if [ -z "$NDK_ROOT" ] || [ ! -d "$NDK_ROOT" ]; then
    echo "Error: Android NDK not found. Set ANDROID_NDK_HOME or ANDROID_SDK_ROOT."
    exit 1
fi

CMAKE_BIN="${CMAKE_PATH:-$(command -v cmake || true)}"
if [ -z "$CMAKE_BIN" ] && [ -d "$SDK_ROOT/cmake" ]; then
    CMAKE_VER_DIR="$(ls -1d "$SDK_ROOT/cmake/"* | sort -r | head -n 1)"
    CMAKE_BIN="$CMAKE_VER_DIR/bin/cmake"
fi

if [ -z "$CMAKE_BIN" ] || [ ! -x "$CMAKE_BIN" ]; then
    echo "Error: cmake not found. Install Android CMake or set CMAKE_PATH."
    exit 1
fi

TOOLCHAIN="$NDK_ROOT/build/cmake/android.toolchain.cmake"
LIB_NAME="$(python3 -c "import json; print(json.load(open('$MANIFEST'))['native_runtime']['library_name'])")"

NEEDS_SHARED_STL="$(python3 -c "import json; deps=json.load(open('$MANIFEST')).get('native_runtime',{}).get('dependencies',[]); print('1' if 'libc++_shared.so' in deps else '0')")"
ANDROID_STL="c++_static"
if [ "$NEEDS_SHARED_STL" = "1" ]; then
    ANDROID_STL="c++_shared"
fi

for ABI in arm64-v8a armeabi-v7a x86_64; do
    ABI_BUILD="$EXT_DIR/build/$ABI"
    ABI_OUT="$EXT_DIR/runtime_bundle/adapters/$ABI"
    ABI_JNI="$EXT_DIR/runtime_bundle/jniLibs/$ABI"
    mkdir -p "$ABI_BUILD" "$ABI_OUT"
    if [ "$NEEDS_SHARED_STL" = "1" ]; then
        mkdir -p "$ABI_JNI"
    fi

    "$CMAKE_BIN" -S "$NATIVE_DIR" -B "$ABI_BUILD" \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
        -DANDROID_ABI="$ABI" \
        -DANDROID_PLATFORM=android-24 \
        -DANDROID_STL="$ANDROID_STL" \
        -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-z,max-page-size=16384" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

    "$CMAKE_BIN" --build "$ABI_BUILD" --parallel

    BUILT_SO="$(find "$ABI_BUILD" -name "$LIB_NAME" | head -n 1)"
    if [ -z "$BUILT_SO" ]; then
        echo "Error: Could not find built $LIB_NAME for $ABI"
        exit 1
    fi
    cp -f "$BUILT_SO" "$ABI_OUT/$LIB_NAME"

    if [ "$NEEDS_SHARED_STL" = "1" ]; then
        case "$ABI" in
            arm64-v8a) TRIPLE="aarch64-linux-android" ;;
            armeabi-v7a) TRIPLE="arm-linux-androideabi" ;;
            x86_64) TRIPLE="x86_64-linux-android" ;;
        esac
        PREBUILT_STL="$(find "$NDK_ROOT/toolchains/llvm/prebuilt" -path "*/sysroot/usr/lib/$TRIPLE/libc++_shared.so" | head -n 1)"
        if [ -n "$PREBUILT_STL" ]; then
            cp -f "$PREBUILT_STL" "$ABI_JNI/libc++_shared.so"
        fi
    fi
    echo "Built $LIB_NAME for $ABI ($ANDROID_STL)"
done

echo ""
echo "SUCCESS! Native build completed. Outputs are in $EXT_DIR/runtime_bundle"
