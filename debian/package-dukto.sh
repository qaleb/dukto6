#!/bin/bash

# Dukto6 Debian Package Build Script
# This script builds a .deb package for Dukto6 using cqtdeployer and ImageMagick.
# Author: Caleb Maangi <drmaangi@gmail.com>

set -e  # Exit on any error
set -o pipefail

# === CONFIGURATION ===
APP_NAME="dukto6"
VERSION="6.9.6"
PKG_DIR="${APP_NAME}_${VERSION}"
DIST_DIR="DistributionKit"
DIST_DIR_ABS="${PWD}/${DIST_DIR}"
DEPLOY_BIN="bin/dukto6"
QMAKE_PATH="/home/qaleb/Qt/6.9.0/gcc_64/bin/qmake"
QML_DIR="/home/qaleb/coding/dukto6/build/Desktop_Qt_6_9_0-Release/dukto6/src/qml/dukto6"
ICON_SOURCE="../../debian/dukto-x4.png"
LAUNCH_SCRIPT_SOURCE="${DIST_DIR}/dukto6.sh"

# === FUNCTIONS ===
log() {
    echo -e "\033[1;32m$1\033[0m"
}

warn() {
    echo -e "\033[1;33m$1\033[0m"
}

error_exit() {
    echo -e "\033[1;31m$1\033[0m" >&2
    exit 1
}

# === CHECK DEPENDENCIES ===
command -v cqtdeployer >/dev/null 2>&1 || error_exit "❌ cqtdeployer not found. Please install it."
command -v convert >/dev/null 2>&1 || error_exit "❌ ImageMagick 'convert' not found. Please install it."
command -v dpkg-deb >/dev/null 2>&1 || error_exit "❌ dpkg-deb not found. Please install dpkg."

# === BUILD START ===
log "🔧 Running cqtdeployer..."
cqtdeployer -bin "$DEPLOY_BIN" -qmake "$QMAKE_PATH" -qmlDir "$QML_DIR"

log "🧹 Cleaning previous build..."
rm -rf "$PKG_DIR"

log "📁 Creating package structure..."
mkdir -p "$PKG_DIR/DEBIAN"
mkdir -p "$PKG_DIR/usr/local/dukto"
mkdir -p "$PKG_DIR/usr/share/applications"
mkdir -p "$PKG_DIR/usr/share/icons/hicolor"

# --- Control file ---
log "📄 Creating control file..."
cat <<EOF > "$PKG_DIR/DEBIAN/control"
Package: $APP_NAME
Version: $VERSION
Section: utils
Priority: optional
Architecture: amd64
Maintainer: Caleb Maangi <drmaangi@gmail.com>
Description: Easy multi-platform file and text transfer tool
 Dukto is an easy file transfer tool designed for LAN use.
 You can use it to transfer files from one device to another,
 without worrying about users, permissions, operating systems,
 protocols, clients, servers and so on. Just start Dukto on
 the two devices and transfer files and folders by dragging onto
 its window.
EOF

# --- Copy deployed files ---
log "📂 Copying deployed files..."
for dir in bin lib plugins qml translations; do
    if [ -d "$DIST_DIR_ABS/$dir" ]; then
        cp -r "$DIST_DIR_ABS/$dir" "$PKG_DIR/usr/local/dukto/"
    else
        warn "⚠️ Directory '$dir' not found in $DIST_DIR_ABS"
    fi
done

# --- Launcher script ---
log "📄 Installing launcher script..."
cp "$LAUNCH_SCRIPT_SOURCE" "$PKG_DIR/usr/local/dukto/dukto.sh"
chmod +x "$PKG_DIR/usr/local/dukto/dukto.sh"

# --- Icons ---
log "🎨 Generating icons..."
for size in 16 24 32 48 64 128 256 512; do
    ICON_PATH="$PKG_DIR/usr/share/icons/hicolor/${size}x${size}/apps"
    mkdir -p "$ICON_PATH"
    convert "$ICON_SOURCE" -resize ${size}x${size} "$ICON_PATH/dukto.png"
done

# --- .desktop entry ---
log "📝 Creating .desktop entry..."
cat <<EOF > "$PKG_DIR/usr/share/applications/dukto.desktop"
[Desktop Entry]
Name=Dukto
Comment=Easy LAN File Transfer Tool
Exec=/usr/local/dukto/dukto.sh
Icon=dukto
Terminal=false
Type=Application
Categories=Utility;Network;
StartupWMClass=dukto6
MimeType=application/octet-stream;
EOF

# --- Build package ---
log "📦 Building .deb package..."
dpkg-deb --build "$PKG_DIR" || error_exit "❌ Failed to build .deb package"

log "✅ Done! Package created: ${PKG_DIR}.deb"
