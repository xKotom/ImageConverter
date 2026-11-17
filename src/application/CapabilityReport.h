#pragma once
#include "CodecRegistry.h"

#include <string>
#include <vector>

namespace uic {

struct FormatReportRow {
    FormatCapability cap;
    std::string label;
    bool alpha = false;
    bool icc = false;
    bool lossy = false;
    std::string notes; // ограничения и пояснения для пользователя (UTF-8)
};

// Описание реально доступных возможностей — для окна «Поддерживаемые форматы», CLI и подсказок.
// Строится только из того, что подтверждено реестром при запуске.
class CapabilityReport {
public:
    static std::vector<FormatReportRow> rows(const CodecRegistry& registry);   // все известные форматы
    static std::string summaryLine(const CodecRegistry& registry);             // «Чтение: …; запись: …»
    static std::string detailedText(const CodecRegistry& registry);            // таблица для консоли
    static std::string tooltipFor(const FormatReportRow& row);                 // подсказка для выбора формата
};

} // namespace uic
