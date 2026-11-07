#!/usr/bin/env bash
# Compila o APK do Babuun Hollow (Oculus Go, armeabi-v7a) sem Gradle.
# Requer: Android NDK r21+, build-tools (aapt, zipalign, apksigner), android.jar (API 26) e Java.
# Variáveis (opcionais): ANDROID_NDK, BUILD_TOOLS, ANDROID_JAR
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
NDK="${ANDROID_NDK:-/opt/android/android-ndk-r21e}"
BT="${BUILD_TOOLS:-/opt/android/android-11}"
JAR="${ANDROID_JAR:-/opt/android/android-8.0.0/android.jar}"
OUT="$ROOT/build"
VR="$ROOT/third_party/vrapi"
[ -f "$VR/lib/libvrapi.so" ] || "$ROOT/tools/fetch_vrapi.sh"

TC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin"
CXX="$TC/armv7a-linux-androideabi24-clang++"
CC="$TC/armv7a-linux-androideabi24-clang"
mkdir -p "$OUT/obj" "$OUT/apk/lib/armeabi-v7a"

FLAGS="-O2 -fPIC -ffunction-sections -fdata-sections -march=armv7-a -mfloat-abi=softfp -mfpu=neon -DANDROID \
 -I$NDK/sources/android/native_app_glue -I$VR/Include -Wno-unused-parameter"
echo "== compilando"
$CC $FLAGS -c "$NDK/sources/android/native_app_glue/android_native_app_glue.c" -o "$OUT/obj/glue.o"
OBJS="$OUT/obj/glue.o"
for f in "$ROOT"/app/jni/*.cpp; do
  o="$OUT/obj/$(basename "${f%.cpp}").o"
  $CXX $FLAGS -std=c++14 -fno-exceptions -fno-rtti -c "$f" -o "$o" &
  OBJS="$OBJS $o"
done
wait
echo "== linkando"
$CXX -shared -o "$OUT/apk/lib/armeabi-v7a/libbabuunhollow.so" $OBJS \
  -L"$VR/lib" -lvrapi -landroid -llog -lEGL -lGLESv3 -lOpenSLES -lmediandk \
  -Wl,--gc-sections -Wl,-u,ANativeActivity_onCreate -static-libstdc++ -s
cp "$VR/lib/libvrapi.so" "$OUT/apk/lib/armeabi-v7a/"

echo "== empacotando"
rm -f "$OUT/unsigned.apk" "$OUT/aligned.apk"
"$BT/aapt" package -f -M "$ROOT/app/AndroidManifest.xml" -A "$ROOT/app/assets" -I "$JAR" -F "$OUT/unsigned.apk"
(cd "$OUT/apk" && "$BT/aapt" add "$OUT/unsigned.apk" lib/armeabi-v7a/libbabuunhollow.so lib/armeabi-v7a/libvrapi.so >/dev/null)
"$BT/zipalign" -f -p 4 "$OUT/unsigned.apk" "$OUT/aligned.apk"
KS="$ROOT/build/debug.keystore"
[ -f "$KS" ] || keytool -genkeypair -keystore "$KS" -storepass android -keypass android -alias babuun \
  -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Babuun Hollow" >/dev/null 2>&1
mkdir -p "$ROOT/dist"
"$BT/apksigner" sign --ks "$KS" --ks-pass pass:android --key-pass pass:android --out "$ROOT/dist/BabuunHollow.apk" "$OUT/aligned.apk"
"$BT/apksigner" verify "$ROOT/dist/BabuunHollow.apk" && echo "APK pronto: dist/BabuunHollow.apk ($(du -h "$ROOT/dist/BabuunHollow.apk" | cut -f1))"
