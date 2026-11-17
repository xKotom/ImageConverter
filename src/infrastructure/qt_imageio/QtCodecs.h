#pragma once
#include "application/CodecRegistry.h"

namespace uic {

// Регистрирует Qt Image I/O адаптеры. Фактический набор форматов определяется
// списком плагинов, доступных при запуске (QImageReader/QImageWriter::supportedImageFormats).
void registerQtCodecs(CodecRegistry& registry);

// Текстовое описание возможностей («Чтение: ...; Запись: ...») для строки состояния/CLI.
std::string describeCapabilities(const CodecRegistry& registry);

} // namespace uic
