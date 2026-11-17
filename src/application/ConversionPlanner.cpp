#include "ConversionPlanner.h"
#include <cstdio>

namespace uic {

ConversionPlan ConversionPlanner::plan(const FileInfo& info, const ConversionSettings& s) const {
    ConversionPlan p;
    const auto& src = traits(info.actualFormat);
    const auto& dst = traits(s.targetFormat);

    if (!registry_.canRead(info.actualFormat)) {
        p.rejectReason = std::string("Нет декодера для формата ") + src.label;
        return p;
    }
    if (s.targetFormat == ImageFormat::Unknown || !registry_.canWrite(s.targetFormat)) {
        p.rejectReason = std::string("Нет кодировщика для формата ") + dst.label;
        return p;
    }
    if (static_cast<long long>(info.width) * info.height > kMaxPixels) {
        p.rejectReason = "Изображение слишком большое для обработки (более 150 Мпикс)";
        return p;
    }

    // ---- анимация / несколько страниц ----
    if (info.animated()) {
        const std::string n = std::to_string(info.frameCount);
        if (s.animation == AnimationPolicy::Reject) {
            p.rejectReason = "Файл содержит несколько кадров (" + n + "); операция отклонена настройкой";
            return p;
        }
        const bool canReadAll = registry_.sequenceDecoderFor(info.actualFormat) != nullptr;
        const bool canWriteAll = registry_.sequenceEncoderFor(s.targetFormat) != nullptr;
        const long long totalBytes = static_cast<long long>(info.width) * info.height * 4 * info.frameCount;
        if (s.animation == AnimationPolicy::AllFrames && canReadAll && canWriteAll) {
            if (totalBytes > kMaxSequenceBytes) {
                p.rejectReason = "Все " + n + " кадров не помещаются в допустимый объём памяти; "
                                 "выберите «только первый кадр»";
                return p;
            }
            p.allFrames = true;
            p.framesToWrite = info.frameCount;
        } else {
            std::string why = !canReadAll ? std::string("декодер ") + src.label + " читает один кадр"
                              : !canWriteAll ? std::string("формат ") + dst.label + " не поддерживает анимацию и многостраничность"
                                             : "выбрана настройка «только первый кадр»";
            p.warnings.push_back("Будет сохранён только первый кадр из " + n + " (" + why + ")");
        }
    }

    if (s.targetFormat == ImageFormat::Gif)
        p.warnings.push_back("GIF: палитра ограничена 256 цветами, прозрачность однобитная");
    if (info.extensionMismatch())
        p.warnings.push_back("Расширение «." + info.extensionHint + "» не соответствует содержимому (" +
                             src.label + "); формат определён по содержимому");
    if (info.hasAlpha && !dst.supportsAlpha) {
        char bg[16];
        std::snprintf(bg, sizeof bg, "#%06X", s.backgroundRgb & 0xFFFFFF);
        p.warnings.push_back(std::string("Прозрачность будет потеряна: заливка фоном ") + bg);
    }
    if (dst.lossy && !src.lossy)
        p.warnings.push_back(std::string("Формат ") + dst.label + " сжимает с потерями качества");
    if (info.actualFormat == s.targetFormat)
        p.warnings.push_back("Исходный и целевой формат совпадают: файл будет перекодирован заново");
    if (info.hasIccProfile && !dst.supportsIcc)
        p.warnings.push_back(std::string("Цветовой профиль ICC не поддерживается в ") + dst.label + " и будет потерян");

    // ---- метаданные ----
    const IMetadataService* md = registry_.metadataService();
    const MetadataInfo& m = info.metadata;
    if (m.any()) {
        if (s.metadata == MetadataPolicy::Strip) {
            p.warnings.push_back("Метаданные (EXIF/XMP/IPTC) не будут сохранены — выбрано удаление");
        } else if (!md || !registry_.metadataService()->canWrite(s.targetFormat)) {
            p.warnings.push_back(std::string("Метаданные (EXIF/XMP/IPTC) будут потеряны: формат ") + dst.label +
                                 " не поддерживает их запись");
        } else {
            p.transferMetadata = true;
            if (m.hasGps) {
                p.warnings.push_back(s.metadata == MetadataPolicy::KeepAll
                                         ? "ВНИМАНИЕ: в результат будут перенесены GPS-координаты съёмки"
                                         : "GPS-координаты съёмки будут удалены, остальные метаданные сохранены");
            }
        }
    }
    if (info.exifOrientation != 1)
        p.warnings.push_back("EXIF-ориентация применена к пикселям; в результате она сброшена");

    p.allowed = true;
    return p;
}

} // namespace uic
