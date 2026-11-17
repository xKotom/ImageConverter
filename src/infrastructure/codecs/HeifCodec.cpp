#include "HeifCodec.h"
#include "ImageScale.h"

#include <cstring>
#include <fstream>
#include <libheif/heif.h>
#include <memory>
#include <mutex>

namespace uic {

namespace {

void ensureInit() {
    static std::once_flag once;
    std::call_once(once, [] { heif_init(nullptr); });
}

struct CtxDeleter { void operator()(heif_context* c) const { heif_context_free(c); } };
struct HandleDeleter { void operator()(heif_image_handle* h) const { heif_image_handle_release(h); } };
struct ImageDeleter { void operator()(heif_image* i) const { heif_image_release(i); } };
struct EncDeleter { void operator()(heif_encoder* e) const { heif_encoder_release(e); } };
using CtxPtr = std::unique_ptr<heif_context, CtxDeleter>;
using HandlePtr = std::unique_ptr<heif_image_handle, HandleDeleter>;
using ImagePtr = std::unique_ptr<heif_image, ImageDeleter>;
using EncPtr = std::unique_ptr<heif_encoder, EncDeleter>;

// Чтение через std::ifstream: корректно работает с юникодными путями Windows.
struct FileSource {
    std::ifstream in;
    std::int64_t size = 0;
};
std::int64_t srcPos(void* u) { return static_cast<FileSource*>(u)->in.tellg(); }
int srcRead(void* data, size_t n, void* u) {
    auto* s = static_cast<FileSource*>(u);
    s->in.read(static_cast<char*>(data), static_cast<std::streamsize>(n));
    return s->in.gcount() == static_cast<std::streamsize>(n) ? 0 : -1;
}
int srcSeek(std::int64_t pos, void* u) {
    auto* s = static_cast<FileSource*>(u);
    s->in.clear();
    s->in.seekg(pos);
    return s->in ? 0 : -1;
}
heif_reader_grow_status srcWait(std::int64_t target, void* u) {
    return target <= static_cast<FileSource*>(u)->size ? heif_reader_grow_status_size_reached
                                                         : heif_reader_grow_status_size_beyond_eof;
}

Result<CtxPtr> openContext(const fs::path& path, FileSource& src) {
    ensureInit();
    src.in.open(path, std::ios::binary);
    if (!src.in) return makeError(ErrorCode::AccessDenied, "Не удалось открыть файл");
    src.in.seekg(0, std::ios::end);
    src.size = src.in.tellg();
    src.in.seekg(0);

    static const heif_reader reader = {1, srcPos, srcRead, srcSeek, srcWait, nullptr};
    CtxPtr ctx(heif_context_alloc());
    heif_error e = heif_context_read_from_reader(ctx.get(), &reader, &src, nullptr);
    if (e.code != heif_error_Ok) {
        const bool unsupported = e.code == heif_error_Unsupported_filetype || e.code == heif_error_Plugin_loading_error ||
                                 e.code == heif_error_Unsupported_feature;
        return makeError(unsupported ? ErrorCode::NoDecoder : ErrorCode::CorruptedFile,
                         std::string("HEIF/AVIF: ") + (e.message ? e.message : "ошибка чтения"));
    }
    return ctx;
}

bool haveDecoder(heif_compression_format f) { ensureInit(); return heif_have_decoder_for_format(f); }

bool haveEncoder(heif_compression_format f) {
    ensureInit();
    CtxPtr ctx(heif_context_alloc());
    heif_encoder* enc = nullptr;
    heif_error e = heif_context_get_encoder_for_format(ctx.get(), f, &enc);
    if (e.code != heif_error_Ok || !enc) return false;
    heif_encoder_release(enc);
    return true;
}

} // namespace

std::string HeifDecoder::version() const {
    ensureInit();
    return std::string("libheif-") + heif_get_version();
}

bool HeifDecoder::supports(ImageFormat f) const {
    if (f == ImageFormat::Heif) return haveDecoder(heif_compression_HEVC);
    if (f == ImageFormat::Avif) return haveDecoder(heif_compression_AV1);
    return false;
}

Result<ImageProbe> HeifDecoder::probe(const fs::path& path) {
    FileSource src;
    auto ctx = openContext(path, src);
    if (!ctx) return ctx.error();
    heif_image_handle* raw = nullptr;
    heif_error e = heif_context_get_primary_image_handle(ctx.value().get(), &raw);
    if (e.code != heif_error_Ok) return makeError(ErrorCode::CorruptedFile, std::string("HEIF/AVIF: ") + e.message);
    HandlePtr h(raw);

    ImageProbe p;
    // Размеры уже учитывают преобразования контейнера (irot/imir) — отдельной EXIF-ориентации нет.
    p.width = heif_image_handle_get_width(h.get());
    p.height = heif_image_handle_get_height(h.get());
    p.hasAlpha = heif_image_handle_has_alpha_channel(h.get()) != 0;
    const auto type = heif_image_handle_get_color_profile_type(h.get());
    p.hasIccProfile = type == heif_color_profile_type_prof || type == heif_color_profile_type_rICC;
    p.exifOrientation = 1;
    p.frameCount = 1; // у файла может быть несколько изображений (серия, Live Photo): берётся основное
    if (p.width <= 0 || p.height <= 0) return makeError(ErrorCode::CorruptedFile, "HEIF/AVIF: нулевой размер");
    return p;
}

Result<ImageBuffer> HeifDecoder::decode(const fs::path& path, const DecodeOptions& opt,
                                        const CancellationToken& token, const ProgressSink& progress) {
    if (token.cancelled()) return makeError(ErrorCode::Cancelled, "Операция отменена");
    FileSource src;
    auto ctx = openContext(path, src);
    if (!ctx) return ctx.error();
    heif_image_handle* raw = nullptr;
    heif_error e = heif_context_get_primary_image_handle(ctx.value().get(), &raw);
    if (e.code != heif_error_Ok) return makeError(ErrorCode::CorruptedFile, std::string("HEIF/AVIF: ") + e.message);
    HandlePtr h(raw);

    const bool alpha = heif_image_handle_has_alpha_channel(h.get()) != 0;
    heif_decoding_options* dopt = heif_decoding_options_alloc();
    dopt->convert_hdr_to_8bit = 1;
    dopt->ignore_transformations = opt.autoOrient ? 0 : 1;
    heif_image* rawImg = nullptr;
    e = heif_decode_image(h.get(), &rawImg, heif_colorspace_RGB, heif_chroma_interleaved_RGBA, dopt);
    heif_decoding_options_free(dopt);
    if (e.code != heif_error_Ok)
        return makeError(e.code == heif_error_Unsupported_feature || e.code == heif_error_Decoder_plugin_error
                             ? ErrorCode::NoDecoder : ErrorCode::CorruptedFile,
                         std::string("Не удалось декодировать HEIF/AVIF: ") + e.message);
    ImagePtr img(rawImg);
    if (token.cancelled()) return makeError(ErrorCode::Cancelled, "Операция отменена");

    int stride = 0;
    const std::uint8_t* plane = heif_image_get_plane_readonly(img.get(), heif_channel_interleaved, &stride);
    const int w = heif_image_get_width(img.get(), heif_channel_interleaved);
    const int hgt = heif_image_get_height(img.get(), heif_channel_interleaved);
    if (!plane || w <= 0 || hgt <= 0) return makeError(ErrorCode::CorruptedFile, "HEIF/AVIF: пустое изображение");

    ImageBuffer buf;
    buf.width = w; buf.height = hgt; buf.hasAlpha = alpha;
    buf.rgba.resize(static_cast<size_t>(w) * hgt * 4);
    for (int y = 0; y < hgt; ++y)
        std::memcpy(buf.rgba.data() + static_cast<size_t>(y) * w * 4, plane + static_cast<size_t>(y) * stride,
                    static_cast<size_t>(w) * 4);
    if (!alpha)
        for (size_t i = 3; i < buf.rgba.size(); i += 4) buf.rgba[i] = 255;

    const auto type = heif_image_handle_get_color_profile_type(h.get());
    if (type == heif_color_profile_type_prof || type == heif_color_profile_type_rICC) {
        const size_t n = heif_image_handle_get_raw_color_profile_size(h.get());
        if (n > 0 && n < (64u << 20)) {
            buf.iccProfile.resize(n);
            if (heif_image_handle_get_raw_color_profile(h.get(), buf.iccProfile.data()).code != heif_error_Ok)
                buf.iccProfile.clear();
        }
    }
    if (opt.maxDimension > 0) buf = downscaleToFit(buf, opt.maxDimension);
    if (progress) progress(100);
    return buf;
}

// ---------------- запись ----------------

bool HeifEncoder::supports(ImageFormat f) const {
    if (f == ImageFormat::Avif) return haveEncoder(heif_compression_AV1);
#ifdef UIC_ENABLE_HEIC_ENCODE
    if (f == ImageFormat::Heif) return haveEncoder(heif_compression_HEVC);
#endif
    return false;
}

Status HeifEncoder::encode(const ImageBuffer& buf, const fs::path& path, const EncoderOptions& opt,
                           const CancellationToken& token, const ProgressSink& progress) {
    if (token.cancelled()) return makeError(ErrorCode::Cancelled, "Операция отменена");
    if (!supports(opt.format)) return makeError(ErrorCode::NoEncoder, "Кодировщик недоступен");
    ensureInit();

    CtxPtr ctx(heif_context_alloc());
    heif_encoder* rawEnc = nullptr;
    heif_error e = heif_context_get_encoder_for_format(
        ctx.get(), opt.format == ImageFormat::Avif ? heif_compression_AV1 : heif_compression_HEVC, &rawEnc);
    if (e.code != heif_error_Ok) return makeError(ErrorCode::NoEncoder, std::string("HEIF: ") + e.message);
    EncPtr enc(rawEnc);

    if (opt.quality >= 100) heif_encoder_set_lossless(enc.get(), 1);
    else heif_encoder_set_lossy_quality(enc.get(), std::clamp(opt.quality, 1, 99));
    heif_encoder_set_parameter_integer(enc.get(), "speed", 6); // баланс скорости AV1; если параметра нет — игнорируется

    heif_image* rawImg = nullptr;
    e = heif_image_create(buf.width, buf.height, heif_colorspace_RGB, heif_chroma_interleaved_RGBA, &rawImg);
    if (e.code != heif_error_Ok) return makeError(ErrorCode::EncodeFailed, std::string("HEIF: ") + e.message);
    ImagePtr img(rawImg);
    e = heif_image_add_plane(img.get(), heif_channel_interleaved, buf.width, buf.height, 8);
    if (e.code != heif_error_Ok) return makeError(ErrorCode::EncodeFailed, std::string("HEIF: ") + e.message);
    int stride = 0;
    std::uint8_t* plane = heif_image_get_plane(img.get(), heif_channel_interleaved, &stride);
    if (!plane) return makeError(ErrorCode::EncodeFailed, "HEIF: нет плоскости изображения");
    for (int y = 0; y < buf.height; ++y)
        std::memcpy(plane + static_cast<size_t>(y) * stride, buf.rgba.data() + static_cast<size_t>(y) * buf.width * 4,
                    static_cast<size_t>(buf.width) * 4);
    if (!buf.iccProfile.empty())
        heif_image_set_raw_color_profile(img.get(), "prof", buf.iccProfile.data(), buf.iccProfile.size());
    if (progress) progress(30);

    heif_image_handle* rawHandle = nullptr;
    heif_encoding_options* eo = heif_encoding_options_alloc();
    e = heif_context_encode_image(ctx.get(), img.get(), enc.get(), eo, &rawHandle);
    heif_encoding_options_free(eo);
    if (e.code != heif_error_Ok) return makeError(ErrorCode::EncodeFailed, std::string("Ошибка кодирования HEIF/AVIF: ") + e.message);
    HandlePtr handle(rawHandle);
    if (token.cancelled()) return makeError(ErrorCode::Cancelled, "Операция отменена");

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    heif_writer writer;
    writer.writer_api_version = 1;
    writer.write = [](heif_context*, const void* data, size_t size, void* u) -> heif_error {
        auto* o = static_cast<std::ofstream*>(u);
        o->write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        return *o ? heif_error{heif_error_Ok, heif_suberror_Unspecified, "ok"}
                  : heif_error{heif_error_Encoding_error, heif_suberror_Cannot_write_output_data, "write failed"};
    };
    e = heif_context_write(ctx.get(), &writer, &out);
    out.close();
    if (e.code != heif_error_Ok || !out) return makeError(ErrorCode::IoError, "Не удалось записать файл HEIF/AVIF");
    if (progress) progress(100);
    return Ok{};
}

} // namespace uic
