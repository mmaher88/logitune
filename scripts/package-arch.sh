#!/bin/bash
set -e

# Derive version from the tag when the workflow is triggered by a tag push,
# otherwise fall back to the version in CMakeLists.txt. pkgver forbids
# hyphens and tildes, so pre-release identifiers use dots: v0.3.0-beta.1
# becomes 0.3.0.beta.1.
if [ -n "$GITHUB_REF_NAME" ] && [[ "$GITHUB_REF_NAME" =~ ^v[0-9] ]]; then
    TAG="${GITHUB_REF_NAME#v}"
else
    TAG=$(grep -oP 'project\(logitune VERSION \K[0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt)
fi
VERSION="${TAG//-/.}"
SRCDIR="$(cd "$(dirname "$0")/.." && pwd)"

echo "Building Arch package v$VERSION"

# Create PKGBUILD in source dir (makepkg sets $startdir to this directory)
cat > "$SRCDIR/PKGBUILD" << 'PKGBUILD_EOF'
# Maintainer: Mina Maher <mina.maher88@hotmail.com>
pkgname=logitune
pkgver=VERSION_PLACEHOLDER
pkgrel=1
pkgdesc="Logitech device configurator for Linux — per-app profiles, button remapping, DPI, gestures"
arch=('x86_64')
url="https://github.com/mmaher88/logitune"
license=('GPL-3.0-or-later')
depends=('qt6-base' 'qt6-declarative' 'qt6-svg' 'qt6-5compat' 'systemd-libs')
makedepends=('cmake' 'ninja' 'qt6-tools')
optdepends=('gnome-shell: per-app profile switching on GNOME'
            'gnome-shell-extension-appindicator: system tray icon on GNOME')
source=()

build() {
    cd "$startdir"
    cmake -B build-pkg -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=OFF -Wno-dev
    cmake --build build-pkg -j$(nproc)
}

package() {
    cd "$startdir"
    DESTDIR="$pkgdir" cmake --install build-pkg
    rm -rf build-pkg
}
PKGBUILD_EOF

# Inject version (heredoc was single-quoted to preserve $startdir/$pkgdir)
sed -i "s/VERSION_PLACEHOLDER/$VERSION/" "$SRCDIR/PKGBUILD"

# Build
cd "$SRCDIR"
makepkg -p PKGBUILD -f --noconfirm 2>&1 | tail -5
rm -f PKGBUILD

echo "logitune-${VERSION}-1-x86_64.pkg.tar.zst"
