#pragma once
#include "application/CodecRegistry.h"

namespace uic {

// Регистрирует все адаптеры в порядке приоритета: Qt Image I/O (одиночные кадры, превью),
// затем специализированные — libheif (HEIC/AVIF), giflib (GIF), libwebp (анимированный WEBP),
// libtiff (многостраничный TIFF) — и сервис метаданных exiv2.
// Что из этого реально доступно, определяется при запуске (см. CodecRegistry::capabilities()).
void registerAllCodecs(CodecRegistry& registry);

} // namespace uic
