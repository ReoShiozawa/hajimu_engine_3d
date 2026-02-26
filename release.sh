#!/bin/bash
# =============================================================================
# engine_3d — ローカルビルド + GitHub Release スクリプト
# =============================================================================
set -euo pipefail
cd "$(dirname "$0")"

PLUGIN_NAME="engine_3d"
HJP_FILE="build/${PLUGIN_NAME}.hjp"

VERSION=""
PUSH_ONLY=false
for arg in "$@"; do
  case "$arg" in
    --push) PUSH_ONLY=true ;;
    v*|[0-9]*) VERSION="$arg" ;;
  esac
done

[[ -z "$VERSION" ]] && VERSION=$(jq -r '.version // .バージョン // empty' hajimu.json 2>/dev/null || true)
[[ -z "$VERSION" ]] && { echo "使い方: ./release.sh [バージョン] [--push]"; exit 1; }
[[ "$VERSION" == v* ]] || VERSION="v$VERSION"

echo "=== $PLUGIN_NAME $VERSION リリース ==="

if [[ "$PUSH_ONLY" == false ]]; then
  echo "--- macOS ビルド ---"
  make clean && make
  echo "  → $HJP_FILE"
fi

echo "--- Git push ---"
git add -A
git diff --cached --quiet || git commit -m "release: $PLUGIN_NAME $VERSION"
git push origin HEAD

if git rev-parse "$VERSION" >/dev/null 2>&1; then
  echo "  タグ $VERSION は既に存在します"
else
  git tag -a "$VERSION" -m "$PLUGIN_NAME $VERSION"
fi
git push origin "$VERSION"

if command -v gh >/dev/null 2>&1 && [[ -f "$HJP_FILE" ]]; then
  echo "--- GitHub Release 作成 ---"
  mkdir -p dist
  cp "$HJP_FILE" "dist/${PLUGIN_NAME}-macos.hjp"
  gh release create "$VERSION" "dist/${PLUGIN_NAME}-macos.hjp" \
    --title "$PLUGIN_NAME $VERSION" --generate-notes 2>/dev/null || \
  gh release upload "$VERSION" "dist/${PLUGIN_NAME}-macos.hjp" --clobber 2>/dev/null || \
  echo "⚠ Release 作成/アップロードに失敗"
fi

echo "=== リリース完了: $PLUGIN_NAME $VERSION ==="
