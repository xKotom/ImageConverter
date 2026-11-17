#!/usr/bin/env bash
# Собирает автономную папку приложения: exe + Qt (windeployqt) + все прочие DLL из MSYS2 (libheif, exiv2, libwebp, ...).
# Использование (из оболочки MSYS2 UCRT64):  scripts/deploy.sh /c/uic-build
set -euo pipefail
BUILD="${1:?укажите папку сборки, например /c/uic-build}"
export PATH="/c/msys64/ucrt64/bin:$PATH"
EXE="$BUILD/UniversalImageConverter.exe"
[ -f "$EXE" ] || { echo "Не найден $EXE — сначала соберите проект"; exit 1; }

windeployqt6 --no-translations "$EXE"

# Остальные зависимости (всё, что exe и плагины подтягивают из ucrt64/bin).
copy_deps() {
  ldd "$1" | awk '/ucrt64\/bin/ {print $3}' | while read -r dll; do
    [ -f "$BUILD/$(basename "$dll")" ] || cp -u "$dll" "$BUILD/"
  done
}
copy_deps "$EXE"
for p in "$BUILD"/imageformats/*.dll "$BUILD"/platforms/*.dll "$BUILD"/styles/*.dll; do
  [ -f "$p" ] && copy_deps "$p"
done
# вторым проходом: зависимости только что скопированных DLL
for d in "$BUILD"/*.dll; do copy_deps "$d"; done
echo "Готово: $BUILD (можно запускать UniversalImageConverter.exe двойным щелчком)"
