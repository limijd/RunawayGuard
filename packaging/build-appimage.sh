#!/bin/bash
# Build AppImage for RunawayGuard
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
VERSION="0.1.0"
BUILD_DIR="${SCRIPT_DIR}/build"
APPDIR="${BUILD_DIR}/RunawayGuard.AppDir"

echo "Building RunawayGuard AppImage..."

# Check for appimagetool
if ! command -v appimagetool &> /dev/null; then
    echo "Downloading appimagetool..."
    APPIMAGETOOL="${BUILD_DIR}/appimagetool"
    curl -L "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage" -o "$APPIMAGETOOL"
    chmod +x "$APPIMAGETOOL"
else
    APPIMAGETOOL="appimagetool"
fi

# Clean and create build directory
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib" "$APPDIR/usr/share/icons/hicolor/256x256/apps"

# Copy AppDir template
cp "${SCRIPT_DIR}/AppDir/AppRun" "$APPDIR/"
cp "${SCRIPT_DIR}/AppDir/runaway-guard.desktop" "$APPDIR/"
chmod +x "$APPDIR/AppRun"

# Build daemon
echo "Building daemon..."
cd "$PROJECT_DIR/daemon"
cargo build --release

# Build GUI
echo "Building GUI..."
cd "$PROJECT_DIR/gui"
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Copy binaries
echo "Copying binaries..."
cp "$PROJECT_DIR/target/release/runaway-daemon" "$APPDIR/usr/bin/"
cp "$PROJECT_DIR/gui/build/runaway-gui" "$APPDIR/usr/bin/"

# Bundle Qt libraries (using linuxdeploy if available)
if command -v linuxdeploy &> /dev/null; then
    echo "Bundling dependencies with linuxdeploy..."
    linuxdeploy --appdir "$APPDIR" --plugin qt --output appimage
    exit 0
fi

# Manual library bundling fallback
echo "Bundling libraries manually..."
mkdir -p "$APPDIR/usr/lib"

# Copy required Qt libraries
QT_LIB_PATH=$(pkg-config --variable=libdir Qt6Core 2>/dev/null || echo "/usr/lib/x86_64-linux-gnu")
for lib in libQt6Core libQt6Gui libQt6Widgets libQt6Network libQt6DBus libQt6XcbQpa; do
    if [ -f "${QT_LIB_PATH}/${lib}.so.6" ]; then
        cp "${QT_LIB_PATH}/${lib}.so.6" "$APPDIR/usr/lib/"
    fi
done

# Copy Qt plugins
QT_PLUGIN_PATH=$(pkg-config --variable=plugindir Qt6Core 2>/dev/null || echo "/usr/lib/x86_64-linux-gnu/qt6/plugins")
if [ -d "$QT_PLUGIN_PATH" ]; then
    mkdir -p "$APPDIR/usr/plugins"
    cp -r "$QT_PLUGIN_PATH/platforms" "$APPDIR/usr/plugins/" 2>/dev/null || true
    cp -r "$QT_PLUGIN_PATH/platformthemes" "$APPDIR/usr/plugins/" 2>/dev/null || true
fi

# Create placeholder icon (should be replaced with actual icon)
touch "$APPDIR/runaway-guard.png"
ln -sf runaway-guard.png "$APPDIR/.DirIcon"

# Build AppImage
echo "Creating AppImage..."
cd "$BUILD_DIR"
ARCH=x86_64 "$APPIMAGETOOL" "$APPDIR" "RunawayGuard-${VERSION}-x86_64.AppImage"

echo ""
echo "AppImage created: ${BUILD_DIR}/RunawayGuard-${VERSION}-x86_64.AppImage"
echo ""
echo "To run: chmod +x RunawayGuard-${VERSION}-x86_64.AppImage && ./RunawayGuard-${VERSION}-x86_64.AppImage"
