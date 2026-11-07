#!/usr/bin/env bash
# Baixa os headers do VrApi 1.14 e o libvrapi.so 32-bit (armeabi-v7a) compatível com o Oculus Go.
# Fonte: espelho público do Oculus Mobile SDK (licença "Oculus SDK License", incluída).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/third_party/vrapi"
[ -f "$DEST/lib/libvrapi.so" ] && { echo "VrApi já presente em $DEST"; exit 0; }
TMP="$(mktemp -d)"
git clone --depth 1 --filter=blob:none --sparse https://github.com/tommiph/sormus.git "$TMP/s"
(cd "$TMP/s" && git sparse-checkout set plugins/oculus-go/android/vendor)
mkdir -p "$ROOT/third_party"
cp -r "$TMP/s/plugins/oculus-go/android/vendor/vrapi" "$DEST"
cp -r "$TMP/s/plugins/oculus-go/android/vendor/licenses" "$DEST/licenses"
rm -rf "$TMP"
echo "VrApi instalado em $DEST"
