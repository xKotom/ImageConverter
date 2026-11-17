#include "TiffSequenceEncoder.h"

#include <cstring>
#include <algorithm>
#include <tiffio.h>

namespace uic {

Status TiffSequenceEncoder::encode(const ImageBuffer& buf, const fs::path& path, const EncoderOptions& o,
                                   const CancellationToken& t, const ProgressSink& p) {
    ImageSequence seq;
    seq.frames.push_back(buf);
    seq.delaysMs.push_back(0);
    return encodeSequence(seq, path, o, t, p);
}

Status TiffSequenceEncoder::encodeSequence(const ImageSequence& seq, const fs::path& path, const EncoderOptions&,
                                           const CancellationToken& token, const ProgressSink& progress) {
    if (seq.frames.empty()) return makeError(ErrorCode::InvalidInput, "Нет кадров для записи");
#ifdef _WIN32
    TIFF* tif = TIFFOpenW(path.c_str(), "w");
#else
    TIFF* tif = TIFFOpen(path.c_str(), "w");
#endif
    if (!tif) return makeError(ErrorCode::AccessDenied, "Не удалось открыть временный файл для записи TIFF");

    const auto pages = static_cast<std::uint16_t>(std::min<size_t>(seq.frames.size(), 65535));
    for (std::uint16_t i = 0; i < pages; ++i) {
        if (token.cancelled()) { TIFFClose(tif); return makeError(ErrorCode::Cancelled, "Операция отменена"); }
        const ImageBuffer& f = seq.frames[i];
        const int spp = f.hasAlpha ? 4 : 3;
        TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, static_cast<std::uint32_t>(f.width));
        TIFFSetField(tif, TIFFTAG_IMAGELENGTH, static_cast<std::uint32_t>(f.height));
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, spp);
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
        TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_ADOBE_DEFLATE);
        TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, 0));
        if (spp == 4) {
            const std::uint16_t extra[1] = {EXTRASAMPLE_UNASSALPHA};
            TIFFSetField(tif, TIFFTAG_EXTRASAMPLES, 1, extra);
        }
        if (pages > 1) {
            TIFFSetField(tif, TIFFTAG_SUBFILETYPE, FILETYPE_PAGE);
            TIFFSetField(tif, TIFFTAG_PAGENUMBER, i, pages);
        }
        if (i == 0 && !f.iccProfile.empty())
            TIFFSetField(tif, TIFFTAG_ICCPROFILE, static_cast<std::uint32_t>(f.iccProfile.size()), f.iccProfile.data());

        std::vector<std::uint8_t> row(static_cast<size_t>(f.width) * spp);
        for (int y = 0; y < f.height; ++y) {
            const std::uint8_t* src = f.rgba.data() + static_cast<size_t>(y) * f.width * 4;
            if (spp == 4) std::memcpy(row.data(), src, row.size());
            else for (int x = 0; x < f.width; ++x) std::memcpy(&row[static_cast<size_t>(x) * 3], src + static_cast<size_t>(x) * 4, 3);
            if (TIFFWriteScanline(tif, row.data(), static_cast<std::uint32_t>(y), 0) < 0) {
                TIFFClose(tif);
                return makeError(ErrorCode::EncodeFailed, "Ошибка записи строки TIFF");
            }
        }
        if (!TIFFWriteDirectory(tif)) { TIFFClose(tif); return makeError(ErrorCode::EncodeFailed, "Ошибка записи страницы TIFF"); }
        if (progress) progress((i + 1) * 100 / pages);
    }
    TIFFClose(tif);
    return Ok{};
}

} // namespace uic
