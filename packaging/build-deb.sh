#!/bin/bash
# Build .deb package for RunawayGuard
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
VERSION="0.1.0"
PACKAGE_NAME="runaway-guard_${VERSION}_amd64"
BUILD_DIR="${SCRIPT_DIR}/build"
DEB_DIR="${BUILD_DIR}/${PACKAGE_NAME}"

echo "Building RunawayGuard .deb package..."

# Clean and create build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Copy deb structure
cp -r "${SCRIPT_DIR}/deb" "$DEB_DIR"

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
cp "$PROJECT_DIR/target/release/runaway-daemon" "$DEB_DIR/usr/bin/"
cp "$PROJECT_DIR/gui/build/runaway-gui" "$DEB_DIR/usr/bin/"

# Copy config
cp "$PROJECT_DIR/config/default.toml" "$DEB_DIR/etc/runaway-guard/config.toml"

# Copy systemd service
cp "$PROJECT_DIR/scripts/runaway-guard.service" "$DEB_DIR/lib/systemd/user/"

# Set permissions
chmod 755 "$DEB_DIR/usr/bin/runaway-daemon"
chmod 755 "$DEB_DIR/usr/bin/runaway-gui"
chmod 755 "$DEB_DIR/DEBIAN/postinst"
chmod 755 "$DEB_DIR/DEBIAN/prerm"
chmod 644 "$DEB_DIR/etc/runaway-guard/config.toml"
chmod 644 "$DEB_DIR/lib/systemd/user/runaway-guard.service"
chmod 644 "$DEB_DIR/usr/share/applications/runaway-guard.desktop"

# Build package
echo "Building .deb package..."
cd "$BUILD_DIR"
dpkg-deb --build "$PACKAGE_NAME"

echo "Package created: ${BUILD_DIR}/${PACKAGE_NAME}.deb"
echo ""
echo "To install: sudo dpkg -i ${BUILD_DIR}/${PACKAGE_NAME}.deb"
echo "To install with dependencies: sudo apt install -f"
