#!/bin/bash

echo "==================================================="
echo "  InstalinkSDK - Extension Packager (Linux/macOS)  "
echo "==================================================="

if [ -z "$1" ]; then
    echo "Usage: ./pack-extension.sh <path_to_extension_folder>"
    echo "Example: ./pack-extension.sh ../my-extension/"
    exit 1
fi

EXT_DIR="$1"

if [ ! -f "$EXT_DIR/manifest.json" ]; then
    echo "Error: Could not find manifest.json in $EXT_DIR"
    exit 1
fi

EXT_NAME=$(basename $(realpath "$EXT_DIR"))
VERSION=$(cat "$EXT_DIR/manifest.json" | grep -o '"version": "[^"]*' | cut -d'"' -f4)

if [ -z "$VERSION" ]; then
    VERSION="1.0.0"
fi

if grep -q '"runtime_mode": *"native"' "$EXT_DIR/manifest.json" || grep -q '"runtime_mode": *"hybrid"' "$EXT_DIR/manifest.json"; then
    if [ ! -d "$EXT_DIR/runtime_bundle" ]; then
        echo "Error: 'runtime_bundle' folder not found. For native extensions, you must compile the C++ code successfully before packing. The extension would be empty otherwise."
        exit 1
    fi
fi

INS_NAME="${EXT_NAME}-${VERSION}.ins"
DIST_DIR="$EXT_DIR/dist"

mkdir -p "$DIST_DIR"
rm -f "$DIST_DIR/$INS_NAME"

echo "Packaging extension to $DIST_DIR/$INS_NAME ..."

cd "$EXT_DIR"
zip -r "dist/$INS_NAME" . -x "native/*" "build/*" "dist/*" ".git/*" "*.DS_Store" "CMakeLists.txt" "*.cpp" "*.h"

if [ $? -eq 0 ]; then
    echo ""
    echo "SUCCESS! Extension packaged successfully."
    echo "File: $DIST_DIR/$INS_NAME"
    echo "==================================================="
else
    echo "Error packaging extension."
    exit 1
fi


