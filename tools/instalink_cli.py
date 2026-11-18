#!/usr/bin/env python3
import argparse
import os
import sys
import shutil
import json
import zipfile
import subprocess
from pathlib import Path

def get_sdk_root():
    return Path(__file__).resolve().parent.parent

def print_success(msg):
    print(f"\033[92m[SUCCESS]\033[0m {msg}")

def print_error(msg):
    print(f"\033[91m[ERROR]\033[0m {msg}")
    sys.exit(1)

def print_info(msg):
    print(f"\033[94m[INFO]\033[0m {msg}")

def cmd_create(args):
    sdk_root = get_sdk_root()
    templates_dir = sdk_root / "templates"
    
    # Map type to template
    template_map = {
        "web": "web_basic",
        "native": "native_basic",
        "game": "immersive_game"
    }
    
    template_name = template_map.get(args.type)
    if not template_name:
        print_error(f"Invalid type '{args.type}'. Must be web, native, or game.")
        
    src = templates_dir / template_name
    dest = Path(args.name).resolve()
    
    if not src.exists():
        print_error(f"Template '{template_name}' not found at {src}")
        
    if dest.exists():
        print_error(f"Directory '{dest}' already exists!")
        
    shutil.copytree(src, dest)
    
    # Update manifest.json name and id
    manifest_path = dest / "manifest.json"
    if manifest_path.exists():
        with open(manifest_path, 'r', encoding='utf-8') as f:
            manifest = json.load(f)
        
        manifest['id'] = f"com.developer.{args.name.replace('-', '_').lower()}"
        manifest['name'] = args.name.replace('-', ' ').title()
        
        with open(manifest_path, 'w', encoding='utf-8') as f:
            json.dump(manifest, f, indent=2)
            
    print_success(f"Created {args.type} extension at {dest}")

def _resolve_ndk_and_cmake(config):
    ndk_home = config.get("ANDROID_NDK_HOME", "")
    cmake_path = config.get("CMAKE_PATH", "")

    sdk_candidates = [
        os.environ.get("ANDROID_SDK_ROOT", ""),
        os.environ.get("ANDROID_HOME", ""),
        os.path.join(os.environ.get("LOCALAPPDATA", ""), "Android", "Sdk") if os.environ.get("LOCALAPPDATA") else "",
        os.path.expanduser("~/Android/Sdk"),
        os.path.expanduser("~/Library/Android/sdk"),
    ]
    sdk_root = next((p for p in sdk_candidates if p and os.path.isdir(p)), "")

    if not ndk_home or not os.path.exists(ndk_home):
        env_ndk = os.environ.get("ANDROID_NDK_HOME", "")
        if env_ndk and os.path.exists(env_ndk):
            ndk_home = env_ndk
        elif sdk_root and os.path.isdir(os.path.join(sdk_root, "ndk")):
            versions = sorted(Path(sdk_root, "ndk").iterdir(), key=lambda p: p.name, reverse=True)
            if versions:
                ndk_home = str(versions[0])

    if not cmake_path or not os.path.exists(cmake_path):
        exe_name = "cmake.exe" if sys.platform.startswith("win") else "cmake"
        if sdk_root and os.path.isdir(os.path.join(sdk_root, "cmake")):
            versions = sorted(Path(sdk_root, "cmake").iterdir(), key=lambda p: p.name, reverse=True)
            for ver in versions:
                candidate = ver / "bin" / exe_name
                if candidate.is_file():
                    cmake_path = str(candidate)
                    break
        if not cmake_path or not os.path.exists(cmake_path):
            cmake_path = shutil.which("cmake") or ""

    return ndk_home, cmake_path

def cmd_build(args):
    target_dir = Path(args.path).resolve()
    native_dir = target_dir / "native"
    manifest_path = target_dir / "manifest.json"
    if not (native_dir / "CMakeLists.txt").exists():
        print_error("No CMakeLists.txt found in native folder. This does not appear to be a native extension.")
    if not manifest_path.exists():
        print_error(f"manifest.json not found in {target_dir}")

    with open(manifest_path, 'r', encoding='utf-8') as f:
        manifest = json.load(f)

    native_runtime = manifest.get("native_runtime", {})
    lib_name = native_runtime.get("library_name", "")
    deps = native_runtime.get("dependencies", []) or []
    needs_shared_stl = "libc++_shared.so" in deps
    android_stl = "c++_shared" if needs_shared_stl else "c++_static"

    print_info(f"Building native extension in {target_dir} ({android_stl})...")

    sdk_root = get_sdk_root()
    config_path = sdk_root / "config.json"
    example_config_path = sdk_root / "config.example.json"

    config = {}
    if config_path.exists():
        with open(config_path, 'r', encoding='utf-8') as f:
            config = json.load(f)
    elif example_config_path.exists():
        shutil.copy(example_config_path, config_path)
        print_info("Generated config.json from config.example.json.")
        with open(config_path, 'r', encoding='utf-8') as f:
            config = json.load(f)

    ndk_home, cmake_path = _resolve_ndk_and_cmake(config)
    if not ndk_home or not os.path.exists(ndk_home):
        print_error(f"Android NDK not found. Set ANDROID_SDK_ROOT/ANDROID_NDK_HOME or update config.json in {sdk_root}.")
    if not cmake_path or not os.path.exists(cmake_path):
        print_error("CMake executable not found. Install Android SDK CMake or update config.json.")

    toolchain_file = Path(ndk_home) / "build" / "cmake" / "android.toolchain.cmake"
    ninja_name = "ninja.exe" if sys.platform.startswith('win') else "ninja"
    ninja_path = Path(cmake_path).parent / ninja_name

    abi_triples = {
        "arm64-v8a": "aarch64-linux-android",
        "armeabi-v7a": "arm-linux-androideabi",
        "x86_64": "x86_64-linux-android",
    }

    for abi, triple in abi_triples.items():
        abi_build_dir = target_dir / "build" / abi
        abi_build_dir.mkdir(parents=True, exist_ok=True)
        out_adapter_dir = target_dir / "runtime_bundle" / "adapters" / abi
        out_adapter_dir.mkdir(parents=True, exist_ok=True)

        cmake_cmd = [
            cmake_path,
            "-S", str(native_dir),
            "-B", str(abi_build_dir),
            f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}",
            f"-DANDROID_ABI={abi}",
            "-DANDROID_PLATFORM=android-24",
            f"-DANDROID_STL={android_stl}",
            "-DCMAKE_SHARED_LINKER_FLAGS=-Wl,-z,max-page-size=16384",
            "-DCMAKE_BUILD_TYPE=Release",
        ]
        if ninja_path.is_file():
            cmake_cmd.extend(["-G", "Ninja", f"-DCMAKE_MAKE_PROGRAM={ninja_path}"])

        print_info(f"Building {abi}...")
        try:
            subprocess.run(cmake_cmd, check=True)
            subprocess.run([cmake_path, "--build", str(abi_build_dir), "--parallel"], check=True)
        except subprocess.CalledProcessError:
            print_error(f"Compilation failed for {abi}.")

        so_matches = list(abi_build_dir.rglob(lib_name)) if lib_name else list(abi_build_dir.rglob("*.so"))
        if not so_matches:
            print_error(f"Compiled library '{lib_name}' not found in {abi_build_dir}")
        shutil.copy(so_matches[0], out_adapter_dir / so_matches[0].name)

        if needs_shared_stl:
            stl_candidates = list(Path(ndk_home, "toolchains", "llvm", "prebuilt").rglob(f"{triple}/libc++_shared.so"))
            if stl_candidates:
                out_jni_dir = target_dir / "runtime_bundle" / "jniLibs" / abi
                out_jni_dir.mkdir(parents=True, exist_ok=True)
                shutil.copy(stl_candidates[0], out_jni_dir / "libc++_shared.so")

    print_success("Native build completed for arm64-v8a, armeabi-v7a, x86_64.")

def cmd_pack(args):
    target_dir = Path(args.path).resolve()
    manifest_path = target_dir / "manifest.json"
    
    if not manifest_path.exists():
        print_error(f"manifest.json not found in {target_dir}")
        
    with open(manifest_path, 'r', encoding='utf-8') as f:
        try:
            manifest = json.load(f)
        except json.JSONDecodeError:
            print_error("manifest.json is not valid JSON")
            
    ext_id = manifest.get('id', 'unknown_extension')
    version = manifest.get('version', '1.0.0')
    
    dist_dir = target_dir / "dist"
    dist_dir.mkdir(exist_ok=True)
    
    ins_name = f"{ext_id}_v{version}.ins"
    ins_path = dist_dir / ins_name
    
    print_info(f"Packing extension to {ins_path}...")
    
    with zipfile.ZipFile(ins_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for root, dirs, files in os.walk(target_dir):
            parts = Path(root).parts
            # Exclude dist, .git, and the native source code folder
            if 'dist' in parts or '.git' in parts or 'native' in parts or 'build' in parts:
                continue
            for file in files:
                file_path = Path(root) / file
                arcname = file_path.relative_to(target_dir)
                zipf.write(file_path, arcname)
                
    print_success(f"Extension packaged successfully: {ins_path}")

def main():
    parser = argparse.ArgumentParser(description="InstalinkSDK Command Line Interface")
    subparsers = parser.add_subparsers(dest="command", help="Available commands")
    
    # Create command
    parser_create = subparsers.add_parser("create", help="Create a new extension")
    parser_create.add_argument("name", help="Name of the extension directory")
    parser_create.add_argument("--type", choices=["web", "native", "game"], default="web", help="Type of extension to create")
    
    # Build command
    parser_build = subparsers.add_parser("build", help="Build a native extension")
    parser_build.add_argument("path", help="Path to the extension directory")
    
    # Pack command
    parser_pack = subparsers.add_parser("pack", help="Pack an extension for publishing")
    parser_pack.add_argument("path", help="Path to the extension directory")
    
    args = parser.parse_args()
    
    if args.command == "create":
        cmd_create(args)
    elif args.command == "build":
        cmd_build(args)
    elif args.command == "pack":
        cmd_pack(args)
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
