#!/usr/bin/env bash

set -e

# -----------------------------
# Usage help
# -----------------------------
usage() {
    echo "Usage: $0 <icon.png> <output-directory>"
    echo "Example: $0 ../Packaging/icon.png ../Packaging"
    exit 1
}

# -----------------------------
# Validate arguments
# -----------------------------
if [[ $# -ne 2 ]]; then
    usage
fi

ICON_SRC="$1"
OUTPUT_DIR="$2"
ICONSET_DIR="$OUTPUT_DIR/icon.iconset"
ICON_DST="$OUTPUT_DIR/icon.icns"

if [[ ! -f "$ICON_SRC" ]]; then
    echo "Error: Source file '$ICON_SRC' does not exist."
    exit 1
fi

mkdir -p "$ICONSET_DIR"

# -----------------------------
# Generate iconset
# -----------------------------
echo "Generating iconset in: $ICONSET_DIR"

sips -z 16 16     "$ICON_SRC" --out "$ICONSET_DIR/icon_16x16.png"
sips -z 32 32     "$ICON_SRC" --out "$ICONSET_DIR/icon_16x16@2x.png"
sips -z 32 32     "$ICON_SRC" --out "$ICONSET_DIR/icon_32x32.png"
sips -z 64 64     "$ICON_SRC" --out "$ICONSET_DIR/icon_32x32@2x.png"
sips -z 128 128   "$ICON_SRC" --out "$ICONSET_DIR/icon_128x128.png"
sips -z 256 256   "$ICON_SRC" --out "$ICONSET_DIR/icon_128x128@2x.png"
sips -z 256 256   "$ICON_SRC" --out "$ICONSET_DIR/icon_256x256.png"
sips -z 512 512   "$ICON_SRC" --out "$ICONSET_DIR/icon_256x256@2x.png"
sips -z 512 512   "$ICON_SRC" --out "$ICONSET_DIR/icon_512x512.png"
sips -z 1024 1024 "$ICON_SRC" --out "$ICONSET_DIR/icon_512x512@2x.png"

# -----------------------------
# Convert to .icns
# -----------------------------
echo "Creating .icns file: $ICON_DST"
iconutil -c icns "$ICONSET_DIR" -o "$ICON_DST"

echo "✅ Done: $ICON_DST"
