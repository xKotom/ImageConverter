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
    if (info.animated()) {
        if (s.animation == AnimationPolicy::Reject) {
            p.rejectReason = "Файл анимированный (" + std::to_string(info.frameCount) +
                             " кадр.), а анимация не поддерживается; операция отклонена настройкой";
            return p;
        }
        p.warnings.push_back("Анимация не поддерживается: будет сохранён только первый кадр из " +
                             std::to_string(info.frameCount));
    }
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
    p.warnings.push_back("Метаданные (EXIF, GPS) не переносятся; ориентация применена к пикселям");

    p.allowed = true;
    return p;
}

} // namespace uic
