#!/bin/bash
set -e

# Derive version from the tag when the workflow is triggered by a tag push,
# otherwise fall back to the version in CMakeLists.txt. rpm 4.10+ treats
# tilde as a pre-release separator that sorts before digits, so 0.3.0~beta.1
# sorts before 0.3.0.
if [ -n "$GITHUB_REF_NAME" ] && [[ "$GITHUB_REF_NAME" =~ ^v[0-9] ]]; then
    TAG="${GITHUB_REF_NAME#v}"
else
    TAG=$(grep -oP 'project\(logitune VERSION \K[0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt)
fi
VERSION="${TAG//-/\~}"

echo "Building .rpm package v$VERSION"

# Build release
cmake -B build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=OFF -Wno-dev > /dev/null 2>&1
cmake --build build-release -j$(nproc)

# Install to staging dir
DESTDIR="/tmp/logitune-rpm" cmake --install build-release

# Create spec file
mkdir -p ~/rpmbuild/{SPECS,BUILDROOT}
cat > ~/rpmbuild/SPECS/logitune.spec << EOF
Name:           logitune
Version:        $VERSION
Release:        1%{?dist}
Summary:        Logitech device configurator for Linux
License:        GPL-3.0-or-later
URL:            https://github.com/mmaher88/logitune
Recommends:     gnome-shell-extension-appindicator

%description
Configure Logitech HID++ peripherals (MX Master 3S and more).
Per-app profiles, button remapping, gestures, DPI, SmartShift,
scroll configuration, and thumb wheel modes.
Supports KDE Plasma 6 and GNOME 42+ Wayland.

%install
cp -a /tmp/logitune-rpm/* %{buildroot}/

%files
/usr/bin/logitune
/usr/lib/udev/rules.d/71-logitune.rules
/usr/share/applications/logitune.desktop
/etc/xdg/autostart/logitune.desktop
/usr/share/icons/hicolor/scalable/apps/com.logitune.Logitune.svg
# Device descriptors (JSON + images) and the GNOME shell extension live in
# their own subtrees. List the directory so new devices and any additional
# extension resources ship without having to touch this spec.
/usr/share/logitune
/usr/share/gnome-shell/extensions/logitune-focus@logitune.com

%post
udevadm control --reload-rules 2>/dev/null || true
udevadm trigger 2>/dev/null || true
EOF

rpmbuild -bb ~/rpmbuild/SPECS/logitune.spec
rm -rf /tmp/logitune-rpm build-release

echo "RPM built in ~/rpmbuild/RPMS/"
