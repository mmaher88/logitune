#!/bin/bash
set -e

# Derive version from the tag when the workflow is triggered by a tag push,
# otherwise fall back to the version in CMakeLists.txt. Pre-release tags
# like v0.3.0-beta.1 encode as 0.3.0~beta.1 so dpkg sorts them before 0.3.0.
if [ -n "$GITHUB_REF_NAME" ] && [[ "$GITHUB_REF_NAME" =~ ^v[0-9] ]]; then
    TAG="${GITHUB_REF_NAME#v}"
else
    TAG=$(grep -oP 'project\(logitune VERSION \K[0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt)
fi
VERSION="${TAG//-/\~}"
PKGDIR="/tmp/logitune-deb"
ARCH=$(dpkg --print-architecture 2>/dev/null || echo "amd64")

echo "Building .deb package v$VERSION ($ARCH)"

# Build release (offscreen for headless gtest_discover_tests)
export QT_QPA_PLATFORM=offscreen
cmake -B build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=OFF -Wno-dev > /dev/null 2>&1
cmake --build build-release -j$(nproc)

# Create package structure
rm -rf "$PKGDIR"
DESTDIR="$PKGDIR" cmake --install build-release

# Create DEBIAN control
mkdir -p "$PKGDIR/DEBIAN"
cat > "$PKGDIR/DEBIAN/control" << EOF
Package: logitune
Version: $VERSION
Section: utils
Priority: optional
Architecture: $ARCH
Depends: libqt6core6 (>= 6.4), libqt6quick6, libqt6svg6, libqt6dbus6, libqt6widgets6, libudev1, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-dialogs, qml6-module-qtquick-window, qml6-module-qtquick-templates, qml6-module-qtquick-layouts, qml6-module-qtqml, qml6-module-qtqml-workerscript, qml6-module-qt5compat-graphicaleffects
Maintainer: Mina Maher <mina.maher88@hotmail.com>
Description: Logitech device configurator for Linux
 Configure Logitech HID++ peripherals (MX Master 3S and more).
 Per-app profiles, button remapping, gestures, DPI, SmartShift,
 scroll configuration, and thumb wheel modes.
 Supports KDE Plasma 6 and GNOME 42+ Wayland.
Homepage: https://github.com/mmaher88/logitune
EOF

# Post-install: reload udev rules
cat > "$PKGDIR/DEBIAN/postinst" << 'EOF'
#!/bin/sh
udevadm control --reload-rules 2>/dev/null || true
udevadm trigger 2>/dev/null || true
EOF
chmod 755 "$PKGDIR/DEBIAN/postinst"

# Build .deb
dpkg-deb --build "$PKGDIR" "logitune-${VERSION}_${ARCH}.deb"
rm -rf "$PKGDIR" build-release

echo "logitune-${VERSION}_${ARCH}.deb"
