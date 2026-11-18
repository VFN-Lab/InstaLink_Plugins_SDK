import os
import sys
import subprocess
import shutil
from pathlib import Path

def get_sdk_root():
    return Path(__file__).resolve().parent.parent

def build_all():
    sdk_root = get_sdk_root()
    cli_tool = sdk_root / "tools" / "instalink_cli.py"
    releases_dir = sdk_root / "releases"
    
    if releases_dir.exists():
        shutil.rmtree(releases_dir)
    releases_dir.mkdir()
    
    target_dirs = []
    
    # Collect examples
    examples_dir = sdk_root / "examples"
    if examples_dir.exists():
        for d in examples_dir.iterdir():
            if d.is_dir():
                target_dirs.append(d)
                
    # Collect templates
    templates_dir = sdk_root / "templates"
    if templates_dir.exists():
        for d in templates_dir.iterdir():
            if d.is_dir():
                target_dirs.append(d)
                
    print(f"Found {len(target_dirs)} extensions to process.")
    
    for ext_dir in target_dirs:
        print(f"\n--- Processing {ext_dir.name} ---")
        
        # Check if native build is needed
        cmake_file = ext_dir / "native" / "CMakeLists.txt"
        if cmake_file.exists():
            # Skip compiling external Sherpa template when prebuilt jniLibs are not populated yet
            if ext_dir.name == "native_sherpa_tts" and not (ext_dir / "runtime_bundle" / "jniLibs" / "arm64-v8a" / "libonnxruntime.so").exists():
                print("Skipping CMake build for native_sherpa_tts template (external Sherpa jniLibs not present).")
            else:
                subprocess.run([sys.executable, str(cli_tool), "build", str(ext_dir)], check=True)
                build_tmp = ext_dir / "build"
                if build_tmp.exists():
                    shutil.rmtree(build_tmp)

        # Pack
        subprocess.run([sys.executable, str(cli_tool), "pack", str(ext_dir)], check=True)
        
        # Move packages to releases (.ins)
        dist_dir = ext_dir / "dist"
        if dist_dir.exists():
            for f in list(dist_dir.glob("*.ins")):
                shutil.copy(f, releases_dir / f"{ext_dir.name}_{f.name}")
            # Clean up local dist
            shutil.rmtree(dist_dir)
            
    print(f"\nAll extensions built and packaged into {releases_dir}")

if __name__ == "__main__":
    build_all()
