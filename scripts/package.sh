#!/usr/bin/env bash
# Собирает всё, что нужно для установки на другой компьютер:
#   1) ImageConverter-Setup-<версия>.exe  — установщик (ярлыки, удаление, без прав администратора);
#   2) ImageConverter-<версия>-portable.zip — переносимая версия: распаковать и запустить.
#
# Запуск из оболочки MSYS2 UCRT64 (нужны cmake, ninja, qt6, а для установщика — mingw-w64-x86_64-nsis):
#   scripts/package.sh
# Результат: C:/uic-dist/
set -euo pipefail

VERSION="${VERSION:-0.1.0}"
SRC="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="/c/uic-release"          # каталог сборки: только ASCII-путь (см. README)
DIST="/c/uic-dist"
STAGE="$DIST/ImageConverter"
export PATH="/c/msys64/ucrt64/bin:$PATH"

echo "== 1/4  Сборка Release"
cmake -S "$SRC" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG"
cmake --build "$BUILD"

echo "== 2/4  Сборка папки приложения"
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp "$BUILD/ImageConverter.exe" "$STAGE/"
cp "$SRC/resources/icons/app.ico" "$STAGE/app.ico"
"$SRC/scripts/deploy.sh" "$STAGE"

echo "== 3/4  Переносимая версия (zip)"
ZIP="$DIST/ImageConverter-$VERSION-portable.zip"
rm -f "$ZIP"
powershell.exe -NoProfile -Command "Compress-Archive -Path '$(cygpath -w "$STAGE")' -DestinationPath '$(cygpath -w "$ZIP")' -CompressionLevel Optimal"

echo "== 4/4  Установщик"
MAKENSIS="/c/msys64/mingw64/bin/makensis.exe"
if [ -x "$MAKENSIS" ]; then
  cp "$SRC/installer/ImageConverter.nsi" "$DIST/ImageConverter.nsi"   # ASCII-копия: makensis плохо читает кириллицу в пути
  "$MAKENSIS" -V2 -DSTAGE="$(cygpath -w "$STAGE")" -DVERSION="$VERSION" \
      -DOUTFILE="$(cygpath -w "$DIST/ImageConverter-Setup-$VERSION.exe")" "$(cygpath -w "$DIST/ImageConverter.nsi")"
else
  echo "makensis не найден — установщик пропущен (pacman -S mingw-w64-x86_64-nsis)"
fi

echo "== Контрольные суммы"
( cd "$DIST" && sha256sum "ImageConverter-Setup-$VERSION.exe" "ImageConverter-$VERSION-portable.zip" > SHA256SUMS.txt )

echo
echo "Готово. Файлы для передачи:"
ls -lh "$DIST"/*.exe "$DIST"/*.zip "$DIST"/SHA256SUMS.txt 2>/dev/null | awk '{print "  " $5 "  " $9}'
