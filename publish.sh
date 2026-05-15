#!/usr/bin/env bash
set -euo pipefail

DIST_NAME="arena_allocator"
BUILD_DIR="build_publish"
VERSION=$(cat VERSION)
ZIP_FILE="${DIST_NAME}-${VERSION}.zip"

echo "==> Building release..."
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF > /dev/null
cmake --build "$BUILD_DIR" --target arena --config Release --parallel "$(nproc)" > /dev/null

echo "==> Staging..."
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

mkdir -p \
    "$STAGE/$DIST_NAME/lib" \
    "$STAGE/$DIST_NAME/include" \
    "$STAGE/$DIST_NAME/docs"

cp "$BUILD_DIR/libarena.a"  "$STAGE/$DIST_NAME/lib/"
cp -r include/arena          "$STAGE/$DIST_NAME/include/"
cp "$BUILD_DIR/include/arena/version.h" "$STAGE/$DIST_NAME/include/arena/"
rm "$STAGE/$DIST_NAME/include/arena/version.h.in"
cp docs/*.md                 "$STAGE/$DIST_NAME/docs/"
cp README.md                 "$STAGE/$DIST_NAME/"

echo "==> Creating ${ZIP_FILE}..."
rm -f "$ZIP_FILE"
(cd "$STAGE" && zip -r - "$DIST_NAME") > "$ZIP_FILE"

echo "==> Updating dist/..."
rm -rf dist
mkdir dist
cp -r "$STAGE/$DIST_NAME" "dist/${DIST_NAME}-${VERSION}"

echo "==> Done: $ZIP_FILE"
zipinfo "$ZIP_FILE"
