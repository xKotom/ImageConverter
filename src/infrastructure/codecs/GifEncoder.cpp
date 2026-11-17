#include "GifEncoder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <gif_lib.h>
#ifdef _WIN32
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#endif

namespace uic {

namespace {

constexpr int kBins = 32 * 32 * 32;
inline int binOf(unsigned r, unsigned g, unsigned b) { return static_cast<int>(((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)); }

struct Box {
    std::vector<int> bins; // индексы корзин гистограммы
    std::uint64_t count = 0;
};

// Медианное сечение по гистограмме 5 бит на канал.
std::vector<std::array<std::uint8_t, 3>> buildPalette(const std::vector<std::uint32_t>& hist, int maxColors) {
    std::vector<Box> boxes(1);
    for (int i = 0; i < kBins; ++i)
        if (hist[static_cast<size_t>(i)]) { boxes[0].bins.push_back(i); boxes[0].count += hist[static_cast<size_t>(i)]; }
    if (boxes[0].bins.empty()) return {{0, 0, 0}};

    auto channel = [](int bin, int ch) { return ch == 0 ? (bin >> 10) & 31 : ch == 1 ? (bin >> 5) & 31 : bin & 31; };
    while (static_cast<int>(boxes.size()) < maxColors) {
        // делим самый «тяжёлый» ящик, в котором ещё можно что-то разделить
        int best = -1;
        for (size_t i = 0; i < boxes.size(); ++i)
            if (boxes[i].bins.size() > 1 && (best < 0 || boxes[i].count > boxes[static_cast<size_t>(best)].count)) best = static_cast<int>(i);
        if (best < 0) break;
        Box& bx = boxes[static_cast<size_t>(best)];
        int lo[3] = {31, 31, 31}, hi[3] = {0, 0, 0};
        for (int b : bx.bins)
            for (int c = 0; c < 3; ++c) { lo[c] = std::min(lo[c], channel(b, c)); hi[c] = std::max(hi[c], channel(b, c)); }
        int axis = 0;
        for (int c = 1; c < 3; ++c) if (hi[c] - lo[c] > hi[axis] - lo[axis]) axis = c;
        std::sort(bx.bins.begin(), bx.bins.end(), [&](int a, int b) { return channel(a, axis) < channel(b, axis); });
        std::uint64_t acc = 0, half = bx.count / 2;
        size_t cut = 0;
        for (; cut + 1 < bx.bins.size(); ++cut) {
            acc += hist[static_cast<size_t>(bx.bins[cut])];
            if (acc >= half) { ++cut; break; }
        }
        cut = std::clamp<size_t>(cut, 1, bx.bins.size() - 1);
        Box right;
        right.bins.assign(bx.bins.begin() + static_cast<long>(cut), bx.bins.end());
        bx.bins.resize(cut);
        bx.count = 0; for (int b : bx.bins) bx.count += hist[static_cast<size_t>(b)];
        for (int b : right.bins) right.count += hist[static_cast<size_t>(b)];
        boxes.push_back(std::move(right));
    }
    std::vector<std::array<std::uint8_t, 3>> pal;
    for (const Box& bx : boxes) {
        double sum[3] = {0, 0, 0}, w = 0;
        for (int b : bx.bins) {
            const double n = hist[static_cast<size_t>(b)];
            for (int c = 0; c < 3; ++c) sum[c] += n * (channel(b, c) * 8 + 4);
            w += n;
        }
        pal.push_back({static_cast<std::uint8_t>(std::lround(sum[0] / w)),
                       static_cast<std::uint8_t>(std::lround(sum[1] / w)),
                       static_cast<std::uint8_t>(std::lround(sum[2] / w))});
    }
    return pal;
}

int openForWrite(const fs::path& p) {
#ifdef _WIN32
    int fd = -1;
    _wsopen_s(&fd, p.c_str(), _O_WRONLY | _O_BINARY | _O_CREAT | _O_TRUNC, _SH_DENYWR, _S_IREAD | _S_IWRITE);
    return fd;
#else
    return ::open(p.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
#endif
}

Error gifError(const char* what, int code) {
    const char* text = code > 0 ? GifErrorString(code) : nullptr;
    return makeError(ErrorCode::EncodeFailed, std::string("Ошибка записи GIF (") + what + "): " + (text ? text : "неизвестная ошибка"));
}

} // namespace

Status GifEncoder::encode(const ImageBuffer& buf, const fs::path& path, const EncoderOptions& o,
                          const CancellationToken& t, const ProgressSink& p) {
    ImageSequence seq;
    seq.frames.push_back(buf);
    seq.delaysMs.push_back(0);
    return encodeSequence(seq, path, o, t, p);
}

Status GifEncoder::encodeSequence(const ImageSequence& seq, const fs::path& path, const EncoderOptions&,
                                  const CancellationToken& token, const ProgressSink& progress) {
    if (seq.frames.empty()) return makeError(ErrorCode::InvalidInput, "Нет кадров для записи");
    const int W = seq.frames[0].width, H = seq.frames[0].height;
    if (W <= 0 || H <= 0 || W > 65535 || H > 65535)
        return makeError(ErrorCode::EncodeFailed, "Размер изображения недопустим для GIF (максимум 65535 пикселей)");
    for (const auto& f : seq.frames)
        if (f.width != W || f.height != H) return makeError(ErrorCode::InvalidInput, "Кадры имеют разные размеры");

    // --- палитра по всем кадрам ---
    bool anyTransparent = false;
    std::vector<std::uint32_t> hist(kBins, 0);
    for (const auto& f : seq.frames) {
        const std::uint8_t* px = f.rgba.data();
        for (size_t i = 0, n = static_cast<size_t>(W) * H; i < n; ++i, px += 4) {
            if (px[3] < 128) { anyTransparent = true; continue; }
            ++hist[static_cast<size_t>(binOf(px[0], px[1], px[2]))];
        }
        if (token.cancelled()) return makeError(ErrorCode::Cancelled, "Операция отменена");
    }
    const int colorBudget = anyTransparent ? 255 : 256;
    auto palette = buildPalette(hist, colorBudget);
    const int transparentIndex = anyTransparent ? 255 : -1;

    std::vector<std::int16_t> binToIndex(kBins, -1);
    auto indexFor = [&](unsigned r, unsigned g, unsigned b) -> std::uint8_t {
        const int bin = binOf(r, g, b);
        std::int16_t& slot = binToIndex[static_cast<size_t>(bin)];
        if (slot >= 0) return static_cast<std::uint8_t>(slot);
        const int cr = ((bin >> 10) & 31) * 8 + 4, cg = ((bin >> 5) & 31) * 8 + 4, cb = (bin & 31) * 8 + 4;
        int best = 0; long bestD = 1L << 30;
        for (size_t i = 0; i < palette.size(); ++i) {
            const long dr = cr - palette[i][0], dg = cg - palette[i][1], db = cb - palette[i][2];
            const long d = dr * dr * 2 + dg * dg * 4 + db * db * 3; // взвешенно по восприятию
            if (d < bestD) { bestD = d; best = static_cast<int>(i); }
        }
        slot = static_cast<std::int16_t>(best);
        return static_cast<std::uint8_t>(best);
    };

    // --- запись файла ---
    const int fd = openForWrite(path);
    if (fd < 0) return makeError(ErrorCode::AccessDenied, "Не удалось открыть временный файл для записи GIF");
    int err = 0;
    GifFileType* gif = EGifOpenFileHandle(fd, &err);
    if (!gif) {
#ifdef _WIN32
        _close(fd);
#else
        ::close(fd);
#endif
        return gifError("открытие", err);
    }
    auto closeGif = [&] { int e = 0; const int ok = EGifCloseFile(gif, &e); gif = nullptr; return ok == GIF_OK ? 0 : (e ? e : -1); };

    ColorMapObject* map = GifMakeMapObject(256, nullptr);
    if (!map) { closeGif(); return makeError(ErrorCode::EncodeFailed, "Недостаточно памяти для палитры GIF"); }
    for (int i = 0; i < 256; ++i) {
        const bool have = i < static_cast<int>(palette.size());
        map->Colors[i].Red = have ? palette[static_cast<size_t>(i)][0] : 0;
        map->Colors[i].Green = have ? palette[static_cast<size_t>(i)][1] : 0;
        map->Colors[i].Blue = have ? palette[static_cast<size_t>(i)][2] : 0;
    }
    if (EGifPutScreenDesc(gif, W, H, 8, 0, map) != GIF_OK) {
        const int code = gif->Error; GifFreeMapObject(map); closeGif();
        return gifError("заголовок", code);
    }
    GifFreeMapObject(map);

    if (seq.frames.size() > 1) { // расширение NETSCAPE2.0: бесконечный цикл или заданное число повторов
        const unsigned char loop[3] = {1, static_cast<unsigned char>(seq.loopCount & 0xFF),
                                       static_cast<unsigned char>((seq.loopCount >> 8) & 0xFF)};
        EGifPutExtensionLeader(gif, APPLICATION_EXT_FUNC_CODE);
        EGifPutExtensionBlock(gif, 11, "NETSCAPE2.0");
        EGifPutExtensionBlock(gif, 3, loop);
        EGifPutExtensionTrailer(gif);
    }

    std::vector<GifByteType> row(static_cast<size_t>(W));
    for (size_t fi = 0; fi < seq.frames.size(); ++fi) {
        if (token.cancelled()) { closeGif(); return makeError(ErrorCode::Cancelled, "Операция отменена"); }
        const ImageBuffer& f = seq.frames[fi];

        GraphicsControlBlock gcb{};
        gcb.DisposalMode = anyTransparent ? DISPOSE_BACKGROUND : DISPOSE_DO_NOT;
        gcb.UserInputFlag = false;
        const int ms = fi < seq.delaysMs.size() ? seq.delaysMs[fi] : 0;
        gcb.DelayTime = seq.frames.size() > 1 ? std::max(2, (ms + 5) / 10) : 0; // сотые доли секунды
        gcb.TransparentColor = transparentIndex >= 0 ? transparentIndex : NO_TRANSPARENT_COLOR;
        GifByteType ext[4];
        EGifGCBToExtension(&gcb, ext);
        EGifPutExtension(gif, GRAPHICS_EXT_FUNC_CODE, 4, ext);

        if (EGifPutImageDesc(gif, 0, 0, W, H, false, nullptr) != GIF_OK) {
            const int code = gif->Error; closeGif(); return gifError("описание кадра", code);
        }
        for (int y = 0; y < H; ++y) {
            const std::uint8_t* px = f.rgba.data() + static_cast<size_t>(y) * W * 4;
            for (int x = 0; x < W; ++x, px += 4)
                row[static_cast<size_t>(x)] = px[3] < 128 ? static_cast<GifByteType>(transparentIndex < 0 ? 0 : transparentIndex)
                                                          : indexFor(px[0], px[1], px[2]);
            if (EGifPutLine(gif, row.data(), W) != GIF_OK) {
                const int code = gif->Error; closeGif(); return gifError("пиксели", code);
            }
        }
        if (progress) progress(static_cast<int>((fi + 1) * 100 / seq.frames.size()));
    }
    const int e = closeGif();
    if (e != 0) return gifError("закрытие", e);
    return Ok{};
}

} // namespace uic
