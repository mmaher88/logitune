#!/bin/bash
# Usage: scripts/release.sh [major|minor|patch]    (default: patch)
#
# Derives the next version from the latest stable v*-prefixed tag,
# tags the current master SHA, and pushes the tag. That single push
# is the release:
#   - CMake reads the version from the tag at configure time (no
#     CMakeLists.txt edit).
#   - CI builds .deb/.rpm/.pkg.tar.zst, uploads them to a GitHub
#     Release, pushes the PKGBUILD to AUR, and triggers the OBS
#     source-service.
#
# Pre-flight checks: we refuse to release unless the tree is on
# master, clean, and synced with origin. No direct commits to
# master happen here — tagging only.
set -euo pipefail

BUMP="${1:-patch}"

case "$BUMP" in
    major|minor|patch) ;;
    *) echo "usage: $0 [major|minor|patch]" >&2; exit 1 ;;
esac

# Latest stable (non pre-release) tag.
LATEST=$(git describe --tags --abbrev=0 --match 'v[0-9]*' --exclude '*-*' \
    | sed 's/^v//')
IFS='.' read -r MAJ MIN PAT <<<"$LATEST"

case "$BUMP" in
    major) MAJ=$((MAJ + 1)); MIN=0; PAT=0 ;;
    minor) MIN=$((MIN + 1)); PAT=0 ;;
    patch) PAT=$((PAT + 1)) ;;
esac
NEW="$MAJ.$MIN.$PAT"

BRANCH=$(git rev-parse --abbrev-ref HEAD)
if [[ "$BRANCH" != "master" ]]; then
    echo "error: releases cut from master only (currently on $BRANCH)" >&2
    exit 1
fi

if [[ -n "$(git status --porcelain)" ]]; then
    echo "error: working tree has uncommitted changes; commit or stash first" >&2
    exit 1
fi

git fetch origin master --quiet
if [[ "$(git rev-parse HEAD)" != "$(git rev-parse origin/master)" ]]; then
    echo "error: local master is not in sync with origin/master" >&2
    echo "       pull or push first, then retry." >&2
    exit 1
fi

if git rev-parse "v$NEW" >/dev/null 2>&1; then
    echo "error: tag v$NEW already exists locally" >&2
    exit 1
fi
if git ls-remote --tags --exit-code origin "refs/tags/v$NEW" >/dev/null 2>&1; then
    echo "error: tag v$NEW already exists on origin" >&2
    exit 1
fi

echo "Tagging v$NEW   (previous stable: v$LATEST, bump: $BUMP)"
echo "Commit:         $(git rev-parse --short HEAD)"
read -rp "Proceed? [y/N] " reply
[[ "$reply" =~ ^[yY]$ ]] || { echo "aborted"; exit 1; }

git tag -a "v$NEW" -m "v$NEW"
git push origin "v$NEW"

echo ""
echo "Tagged and pushed. Release workflow should now build + publish:"
echo "  https://github.com/mmaher88/logitune/actions/workflows/release.yml"
