#pragma once
#include "domain/Models.h"
#include <string>

namespace uic {

// Порт: чтение сведений о метаданных и их перенос в уже записанный файл результата.
class IMetadataService {
public:
    virtual ~IMetadataService() = default;
    virtual std::string id() const = 0;
    // Есть ли в файле EXIF/XMP/IPTC и GPS (быстро, без декодирования пикселей).
    virtual Result<MetadataInfo> inspect(const fs::path& file) = 0;
    // Может ли сервис записать метаданные в файлы этого формата.
    virtual bool canWrite(ImageFormat) const = 0;
    // Копирует метаданные source -> target (target изменяется на месте).
    // Ориентация сбрасывается в 1, т.к. она уже применена к пикселям.
    virtual Status transfer(const fs::path& source, const fs::path& target, MetadataPolicy) = 0;
};

} // namespace uic
