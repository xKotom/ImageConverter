#include "CapabilityReport.h"

namespace uic {

namespace {
std::string staticNote(ImageFormat f, const FormatCapability& c) {
    switch (f) {
    case ImageFormat::Jpeg: return "Сжатие с потерями, прозрачности нет (при потере заливается фоном).";
    case ImageFormat::Png: return "Без потерь, есть прозрачность.";
    case ImageFormat::Bmp: return "Без сжатия, без прозрачности, ICC и метаданных.";
    case ImageFormat::Webp: return "Есть прозрачность и анимация; качество 100 включает режим без потерь.";
    case ImageFormat::Gif:
        return "Палитра до 256 цветов (медианное сечение, без дизеринга), прозрачность однобитная; анимация.";
    case ImageFormat::Tiff: return "Без потерь; многостраничные файлы сохраняются целиком (Deflate).";
    case ImageFormat::Heif:
        if (c.read && !c.write)
            return "Только чтение (HEVC). Запись отключена: кодировщик x265 распространяется под GPL; "
                   "включается сборкой с -DUIC_ENABLE_HEIC_ENCODE=ON. Из файла берётся основное изображение.";
        return "Из файла берётся основное изображение; метаданные в результат не пишутся.";
    case ImageFormat::Avif:
        return "Кодек AV1 (запись через aom). Из файла берётся основное изображение; метаданные в результат не пишутся.";
    default: return {};
    }
}
} // namespace

std::vector<FormatReportRow> CapabilityReport::rows(const CodecRegistry& reg) {
    std::vector<FormatReportRow> r;
    for (auto f : allKnownFormats()) {
        FormatReportRow row;
        row.cap = reg.capability(f);
        row.label = traits(f).label;
        row.alpha = traits(f).supportsAlpha;
        row.icc = traits(f).supportsIcc;
        row.lossy = traits(f).lossy;
        row.notes = staticNote(f, row.cap);
        if (!row.cap.read && !row.cap.write)
            row.notes = "Недоступен в этой сборке: нет подходящего кодека или плагина. " + row.notes;
        else if (!row.cap.read)
            row.notes = "Только запись. " + row.notes;
        r.push_back(std::move(row));
    }
    return r;
}

std::string CapabilityReport::summaryLine(const CodecRegistry& reg) {
    auto join = [](const std::vector<ImageFormat>& v) {
        std::string s;
        for (auto f : v) { if (!s.empty()) s += ", "; s += traits(f).label; }
        return s.empty() ? std::string("—") : s;
    };
    return "Чтение: " + join(reg.readableFormats()) + "; запись: " + join(reg.writableFormats());
}

std::string CapabilityReport::detailedText(const CodecRegistry& reg) {
    auto yn = [](bool b) { return b ? "да " : "нет"; };
    std::string s = "Формат     Чтение Запись Анимация(чт/зап) Метаданные(зап)\n";
    for (const auto& r : rows(reg)) {
        std::string name = r.label;
        name.resize(std::max<size_t>(10, name.size()), ' ');
        s += name + " ";
        s += std::string(yn(r.cap.read)) + "    " + yn(r.cap.write) + "    ";
        s += std::string(yn(r.cap.readSequence)) + " / " + yn(r.cap.writeSequence) + "         ";
        s += yn(r.cap.writeMetadata);
        s += "\n    " + r.notes + "\n";
    }
    return s;
}

std::string CapabilityReport::tooltipFor(const FormatReportRow& r) {
    std::string s = r.notes;
    s += r.cap.writeSequence ? "\nАнимация и многостраничность: сохраняются все кадры." : "\nАнимация: только первый кадр.";
    s += r.cap.writeMetadata ? "\nМетаданные: переносятся (EXIF/XMP/IPTC)." : "\nМетаданные: не переносятся.";
    return s;
}

} // namespace uic
