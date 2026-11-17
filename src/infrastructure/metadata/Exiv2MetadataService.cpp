#include "Exiv2MetadataService.h"

#include <algorithm>
#include <exiv2/exiv2.hpp>
#include <fstream>
#include <set>

namespace uic {

namespace {

constexpr std::uintmax_t kMaxMetaFileBytes = 256ull * 1024 * 1024;

bool readAll(const fs::path& p, std::vector<Exiv2::byte>& out) {
    std::error_code ec;
    const auto size = fs::file_size(p, ec);
    if (ec || size == 0 || size > kMaxMetaFileBytes) return false;
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    out.resize(static_cast<size_t>(size));
    in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size));
    return in.gcount() == static_cast<std::streamsize>(size);
}

// Структурные теги исходного контейнера: копировать их в другой файл нельзя.
bool isStructuralTag(const Exiv2::Exifdatum& d) {
    if (d.groupName() == "Thumbnail") return true;
    if (d.groupName() == "Image" || d.groupName() == "Photo") {
        static const std::set<uint16_t> bad = {
            0x00fe, 0x00ff, 0x0100, 0x0101, 0x0102, 0x0103, 0x0106, 0x0111, 0x0115, 0x0116, 0x0117,
            0x011c, 0x0140, 0x0142, 0x0143, 0x0144, 0x0145, 0x0152, 0x0153, 0x0201, 0x0202, 0x0212,
            0x8773, /* встроенный ICC — переносится отдельно, в пикселях */
            0xa002, 0xa003 /* PixelX/YDimension */};
        return bad.count(d.tag()) > 0;
    }
    return false;
}

bool isGps(const Exiv2::Exifdatum& d) { return d.groupName() == "GPSInfo"; }
bool isXmpGps(const Exiv2::Xmpdatum& d) {
    return d.groupName() == "exif" && d.tagName().rfind("GPS", 0) == 0;
}

} // namespace

Exiv2MetadataService::Exiv2MetadataService() {
    Exiv2::LogMsg::setLevel(Exiv2::LogMsg::mute);
}

bool Exiv2MetadataService::canWrite(ImageFormat f) const {
    return f == ImageFormat::Jpeg || f == ImageFormat::Png || f == ImageFormat::Webp || f == ImageFormat::Tiff;
}

Result<MetadataInfo> Exiv2MetadataService::inspect(const fs::path& file) {
    MetadataInfo info;
    std::vector<Exiv2::byte> bytes;
    if (!readAll(file, bytes)) return info; // слишком большой/недоступный — «неизвестно», не ошибка
    try {
        auto img = Exiv2::ImageFactory::open(bytes.data(), bytes.size());
        if (!img) return info;
        img->readMetadata();
        const auto& exif = img->exifData();
        info.hasExif = !exif.empty();
        info.hasXmp = !img->xmpData().empty();
        info.hasIptc = !img->iptcData().empty();
        for (const auto& d : exif)
            if (isGps(d)) { info.hasGps = true; break; }
        if (!info.hasGps)
            for (const auto& d : img->xmpData())
                if (isXmpGps(d)) { info.hasGps = true; break; }
    } catch (const std::exception&) {
        // нераспознанный контейнер или повреждённые метаданные: считаем, что их нет
    }
    return info;
}

Status Exiv2MetadataService::transfer(const fs::path& source, const fs::path& target, MetadataPolicy policy) {
    if (policy == MetadataPolicy::Strip) return Ok{};
    std::vector<Exiv2::byte> srcBytes, dstBytes;
    if (!readAll(source, srcBytes)) return makeError(ErrorCode::IoError, "Не удалось прочитать исходный файл для метаданных");
    if (!readAll(target, dstBytes)) return makeError(ErrorCode::IoError, "Не удалось прочитать результат для записи метаданных");

    try {
        auto src = Exiv2::ImageFactory::open(srcBytes.data(), srcBytes.size());
        src->readMetadata();

        Exiv2::ExifData exif;
        for (const auto& d : src->exifData()) {
            if (isStructuralTag(d)) continue;
            if (policy == MetadataPolicy::KeepWithoutGps && isGps(d)) continue;
            if (d.groupName() == "Image" && d.tag() == 0x0112) continue; // ориентация — ниже
            exif.add(d);
        }
        if (!exif.empty()) exif["Exif.Image.Orientation"] = static_cast<uint16_t>(1);

        Exiv2::XmpData xmp;
        for (const auto& d : src->xmpData()) {
            if (policy == MetadataPolicy::KeepWithoutGps && isXmpGps(d)) continue;
            if (d.groupName() == "tiff" && d.tagName() == "Orientation") continue;
            xmp.add(d);
        }
        const Exiv2::IptcData& iptc = src->iptcData();

        if (exif.empty() && xmp.empty() && iptc.empty()) return Ok{}; // переносить нечего

        auto dst = Exiv2::ImageFactory::open(dstBytes.data(), dstBytes.size());
        dst->readMetadata();
        dst->setExifData(exif);
        dst->setXmpData(xmp);
        dst->setIptcData(iptc);
        dst->writeMetadata();

        Exiv2::BasicIo& io = dst->io();
        io.seek(0, Exiv2::BasicIo::beg);
        std::vector<Exiv2::byte> out(static_cast<size_t>(io.size()));
        if (io.read(out.data(), out.size()) != out.size())
            return makeError(ErrorCode::IoError, "Не удалось получить результат записи метаданных");

        std::ofstream f(target, std::ios::binary | std::ios::trunc);
        f.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
        f.close();
        if (!f) return makeError(ErrorCode::IoError, "Не удалось записать метаданные в файл результата");
    } catch (const std::exception& e) {
        return makeError(ErrorCode::EncodeFailed, std::string("Ошибка переноса метаданных: ") + e.what());
    }
    return Ok{};
}

} // namespace uic
