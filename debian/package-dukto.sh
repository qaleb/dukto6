#!/bin/bash

set -e  # Exit on any error

APP_NAME="dukto6"
VERSION="1.0.0"
BUILD_DIR="${APP_NAME}_${VERSION}"
QTDEPLOYER_BIN="appdukto6"
QMAKE_PATH="/home/qaleb/Qt/6.9.0/gcc_64/bin/qmake"
QML_DIR="/home/qaleb/coding/dukto6/build/Desktop_Qt_6_9_0-Release/dukto6/src/qml/dukto6"
ICON_SOURCE="/home/qaleb/coding/dukto6/debian/dukto-x4.png"
LAUNCH_SCRIPT_SOURCE="DistributionKit/appdukto6.sh"

echo "🔧 Running qtdeployer..."
qtdeployer -bin "$QTDEPLOYER_BIN" -qmake "$QMAKE_PATH" -qmlDir "$QML_DIR"

echo "📁 Creating package structure..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR/DEBIAN"
mkdir -p "$BUILD_DIR/usr/local/dukto/bin"
mkdir -p "$BUILD_DIR/usr/local/dukto/lib"
mkdir -p "$BUILD_DIR/usr/local/dukto/plugins"
mkdir -p "$BUILD_DIR/usr/local/dukto/qml"
mkdir -p "$BUILD_DIR/usr/share/applications"
mkdir -p "$BUILD_DIR/usr/share/icons/hicolor"

echo "📄 Creating control file..."
cat <<EOF > "$BUILD_DIR/DEBIAN/control"
Package: $APP_NAME
Version: $VERSION
Section: utils
Priority: optional
Architecture: amd64
Maintainer: Your Name <your@email.com>
Description: Dukto 6 file transfer application
EOF

echo "📄 Creating launcher script..."
cp "$LAUNCH_SCRIPT_SOURCE" "$BUILD_DIR/usr/local/dukto/dukto.sh"
chmod +x "$BUILD_DIR/usr/local/dukto/dukto.sh"

echo "🎨 Generating icons..."
for size in 16 24 32 48 64 128 256 512; do
    ICON_PATH="$BUILD_DIR/usr/share/icons/hicolor/${size}x${size}/apps"
    mkdir -p "$ICON_PATH"
    convert "$ICON_SOURCE" -resize ${size}x${size} "$ICON_PATH/dukto.png"
done

echo "📄 Creating .desktop entry..."
cat <<EOF > "$BUILD_DIR/usr/share/applications/dukto.desktop"
[Desktop Entry]
Name=Dukto
Comment=File Transfer Tool
Exec=/usr/local/dukto/dukto.sh
Icon=dukto
Terminal=false
Type=Application
Categories=Utility;Network;
EOF

echo "📦 Building .deb package..."
dpkg-deb --build "$BUILD_DIR"

echo "✅ Done! Package: ${BUILD_DIR}.deb"
