#pragma once
#include "application/ports/IMetadataService.h"

namespace uic {

// Метаданные (EXIF/XMP/IPTC) на exiv2. Файлы читаются и пишутся через память,
// поэтому юникодные пути Windows не вызывают проблем.
//
// Политика переноса:
//  - Strip          — ничего не переносится;
//  - KeepWithoutGps — переносится всё, кроме координат GPS (EXIF GPSInfo и XMP exif:GPS*);
//  - KeepAll        — переносится всё, включая GPS (явный выбор пользователя).
// Всегда: ориентация в результате = 1 (она уже применена к пикселям), структурные теги
// исходного контейнера (смещения полос TIFF, размеры, встроенная миниатюра) не копируются.
class Exiv2MetadataService : public IMetadataService {
public:
    Exiv2MetadataService();
    std::string id() const override { return "exiv2"; }
    Result<MetadataInfo> inspect(const fs::path& file) override;
    bool canWrite(ImageFormat f) const override;
    Status transfer(const fs::path& source, const fs::path& target, MetadataPolicy policy) override;
};

} // namespace uic
