#include "WebpAnimEncoder.h"

#include <algorithm>
#include <fstream>
#include <webp/encode.h>
#include <webp/mux.h>

namespace uic {

Status WebpAnimEncoder::encode(const ImageBuffer& buf, const fs::path& path, const EncoderOptions& o,
                               const CancellationToken& t, const ProgressSink& p) {
    ImageSequence seq;
    seq.frames.push_back(buf);
    seq.delaysMs.push_back(0);
    return encodeSequence(seq, path, o, t, p);
}

Status WebpAnimEncoder::encodeSequence(const ImageSequence& seq, const fs::path& path, const EncoderOptions& opt,
                                       const CancellationToken& token, const ProgressSink& progress) {
    if (seq.frames.empty()) return makeError(ErrorCode::InvalidInput, "Нет кадров для записи");
    const int W = seq.frames[0].width, H = seq.frames[0].height;
    if (W > 16383 || H > 16383)
        return makeError(ErrorCode::EncodeFailed, "Размер изображения недопустим для WEBP (максимум 16383 пикселя)");

    WebPAnimEncoderOptions ao;
    if (!WebPAnimEncoderOptionsInit(&ao)) return makeError(ErrorCode::Internal, "Несовместимая версия libwebp");
    ao.anim_params.loop_count = std::max(0, seq.loopCount);
    WebPAnimEncoder* enc = WebPAnimEncoderNew(W, H, &ao);
    if (!enc) return makeError(ErrorCode::EncodeFailed, "Не удалось создать WEBP-кодировщик");

    WebPConfig cfg;
    WebPConfigInit(&cfg);
    const int q = std::clamp(opt.quality, 1, 100);
    cfg.quality = static_cast<float>(q);
    cfg.lossless = q == 100 ? 1 : 0;
    cfg.method = 4;
    if (!WebPValidateConfig(&cfg)) { WebPAnimEncoderDelete(enc); return makeError(ErrorCode::Internal, "Некорректные параметры WEBP"); }

    int timestamp = 0;
    for (size_t i = 0; i < seq.frames.size(); ++i) {
        if (token.cancelled()) { WebPAnimEncoderDelete(enc); return makeError(ErrorCode::Cancelled, "Операция отменена"); }
        const ImageBuffer& f = seq.frames[i];
        if (f.width != W || f.height != H) { WebPAnimEncoderDelete(enc); return makeError(ErrorCode::InvalidInput, "Кадры имеют разные размеры"); }
        WebPPicture pic;
        WebPPictureInit(&pic);
        pic.width = W; pic.height = H; pic.use_argb = 1;
        if (!WebPPictureImportRGBA(&pic, f.rgba.data(), f.stride())) {
            WebPPictureFree(&pic); WebPAnimEncoderDelete(enc);
            return makeError(ErrorCode::EncodeFailed, "Не удалось передать кадр кодировщику WEBP");
        }
        const bool ok = WebPAnimEncoderAdd(enc, &pic, timestamp, &cfg) != 0;
        const WebPEncodingError code = pic.error_code;
        WebPPictureFree(&pic);
        if (!ok) {
            const std::string msg = WebPAnimEncoderGetError(enc) ? WebPAnimEncoderGetError(enc) : "";
            WebPAnimEncoderDelete(enc);
            return makeError(ErrorCode::EncodeFailed, "Ошибка кодирования кадра WEBP (" + std::to_string(code) + "): " + msg);
        }
        const int d = i < seq.delaysMs.size() ? seq.delaysMs[i] : 0;
        timestamp += d > 0 ? d : 100;
        if (progress) progress(static_cast<int>((i + 1) * 100 / seq.frames.size()));
    }
    WebPAnimEncoderAdd(enc, nullptr, timestamp, nullptr); // метка конца

    WebPData data;
    WebPDataInit(&data);
    const bool ok = WebPAnimEncoderAssemble(enc, &data) != 0;
    WebPAnimEncoderDelete(enc);
    if (!ok) { WebPDataClear(&data); return makeError(ErrorCode::EncodeFailed, "Не удалось собрать анимированный WEBP"); }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(data.bytes), static_cast<std::streamsize>(data.size));
    out.close();
    WebPDataClear(&data);
    if (!out) return makeError(ErrorCode::IoError, "Не удалось записать WEBP-файл");
    return Ok{};
}

} // namespace uic
