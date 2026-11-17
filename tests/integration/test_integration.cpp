// Интеграционные тесты на реальных кодеках Qt Image I/O: сквозная конвертация, очередь, превью.
#include "application/CodecRegistry.h"
#include "application/CapabilityReport.h"
#include "application/ConversionEngine.h"
#include "application/FileAnalyzer.h"
#include "application/PreviewController.h"
#include "application/QueueManager.h"
#include "infrastructure/codecs/Codecs.h"
#include "infrastructure/qt_imageio/QtConvert.h"

#include <QImageReader>
#include <QImageWriter>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <fstream>

using namespace uic;

namespace {
QImage makeImage(int w, int h, bool alpha) {
    QImage img(w, h, alpha ? QImage::Format_ARGB32 : QImage::Format_RGB32);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img.setPixel(x, y, qRgba(x * 255 / w, y * 255 / h, (x + y) % 255, alpha && x < w / 4 ? 0 : 255));
    return img;
}

std::vector<fs::path> filesIn(const fs::path& d) {
    std::vector<fs::path> r;
    for (auto& e : fs::directory_iterator(d)) r.push_back(e.path());
    return r;
}
} // namespace

class IntegrationTests : public QObject {
    Q_OBJECT
    CodecRegistry reg_;
    QTemporaryDir tmp_;
    fs::path root_;

    fs::path sub(const char* name) {
        fs::path d = root_ / name;
        fs::create_directories(d);
        return d;
    }
    fs::path makeFile(const fs::path& dir, const QString& name, ImageFormat fmt, bool alpha = false, int w = 64, int h = 48) {
        fs::path p = dir / name.toStdU16String();
        QImageWriter wr(toQString(p), traits(fmt).qtName);
        if (!wr.write(makeImage(w, h, alpha))) qFatal("fixture write failed: %s", qPrintable(wr.errorString()));
        return p;
    }

private slots:
    void initTestCase() {
        QVERIFY(tmp_.isValid());
        root_ = fs::path(tmp_.path().toStdU16String());
        registerAllCodecs(reg_);
        for (auto f : {ImageFormat::Jpeg, ImageFormat::Png, ImageFormat::Bmp}) {
            QVERIFY(reg_.canRead(f));
            QVERIFY(reg_.canWrite(f));
        }
        qInfo().noquote() << QString::fromStdString(CapabilityReport::detailedText(reg_));
    }

    void capabilities_reflectActualPlugins() {
        // WEBP в списке доступных — только если плагин реально загружен
        const bool plugin = QImageWriter::supportedImageFormats().contains("webp");
        QCOMPARE(reg_.canWrite(ImageFormat::Webp), plugin);
        // HEIC/AVIF обслуживает libheif; запись HEIC без явного разрешения лицензии (GPL) отключена
        QVERIFY(!reg_.canWrite(ImageFormat::Heif));
        QVERIFY(reg_.canWrite(ImageFormat::Gif)); // GIF пишет giflib, а не Qt
    }

    void convert_allFormatPairs() {
        const fs::path in = sub("pairs_in");
        ConversionEngine engine(reg_);
        FileAnalyzer analyzer(reg_);
        const std::vector<ImageFormat> fmts = reg_.writableFormats();
        for (ImageFormat src : {ImageFormat::Jpeg, ImageFormat::Png, ImageFormat::Bmp, ImageFormat::Webp}) {
            if (!reg_.canWrite(src)) continue;
            const fs::path file = makeFile(in, QString("src_%1.%2").arg(traits(src).name, QString::fromStdString(primaryExtension(src))), src, true);
            const auto sizeBefore = fs::file_size(file);
            for (ImageFormat dst : fmts) {
                if (dst == ImageFormat::Gif) continue;
                auto info = analyzer.analyze(file);
                QVERIFY2(info.ok(), qPrintable(QString::fromStdString(info.ok() ? "" : info.error().message)));
                QCOMPARE(info.value().actualFormat, src);

                ConversionSettings s;
                s.targetFormat = dst;
                s.outputDir = root_ / "pairs_out" / traits(src).name;
                auto r = engine.convert(info.value(), s, CancellationToken(), nullptr);
                QVERIFY2(r.success, qPrintable(QString("%1->%2: %3").arg(traits(src).name, traits(dst).name,
                                               QString::fromStdString(r.error ? r.error->message : ""))));
                QVERIFY(fs::exists(r.outputPath));
                QCOMPARE(QString::fromStdString(r.outputPath.extension().string()),
                         "." + QString::fromStdString(primaryExtension(dst)));
                auto out = analyzer.analyze(r.outputPath);
                QVERIFY(out.ok());
                QCOMPARE(out.value().actualFormat, dst);
                QCOMPARE(out.value().width, 64);
                QCOMPARE(out.value().height, 48);
            }
            QCOMPARE(fs::file_size(file), sizeBefore); // исходник не изменён
        }
        // временные файлы не остаются
        for (auto& d : fs::recursive_directory_iterator(root_ / "pairs_out"))
            QVERIFY(d.path().extension() != ".tmp");
    }

    void convert_alphaToJpegUsesBackground() {
        const fs::path file = makeFile(sub("alpha"), "a.png", ImageFormat::Png, true);
        FileAnalyzer analyzer(reg_);
        auto info = analyzer.analyze(file);
        QVERIFY(info.ok());
        QVERIFY(info.value().hasAlpha);
        ConversionSettings s;
        s.targetFormat = ImageFormat::Jpeg;
        s.quality = 100;
        s.backgroundRgb = 0xFFFFFF;
        auto r = ConversionEngine(reg_).convert(info.value(), s, CancellationToken(), nullptr);
        QVERIFY(r.success);
        bool warned = false;
        for (auto& w : r.warnings) warned |= QString::fromStdString(w).contains("Прозрачность");
        QVERIFY(warned);
        QImage out(toQString(r.outputPath));
        QVERIFY(!out.isNull());
        const QColor c = out.pixelColor(0, 0); // прозрачная область -> белый фон
        QVERIFY2(c.red() > 240 && c.green() > 240 && c.blue() > 240, qPrintable(c.name()));
    }

    void analyze_extensionMismatchUsesContent() {
        const fs::path dir = sub("mismatch");
        const fs::path real = makeFile(dir, "real.png", ImageFormat::Png);
        const fs::path fake = dir / "photo.jpg"; // внутри PNG
        fs::copy_file(real, fake);
        auto info = FileAnalyzer(reg_).analyze(fake);
        QVERIFY(info.ok());
        QCOMPARE(info.value().actualFormat, ImageFormat::Png);
        QVERIFY(info.value().extensionMismatch());

        ConversionSettings s;
        s.targetFormat = ImageFormat::Bmp;
        auto r = ConversionEngine(reg_).convert(info.value(), s, CancellationToken(), nullptr);
        QVERIFY(r.success);
        QCOMPARE(QString::fromStdString(r.outputPath.extension().string()), QString(".bmp"));
        bool warned = false;
        for (auto& w : r.warnings) warned |= QString::fromStdString(w).contains("не соответствует содержимому");
        QVERIFY(warned);
    }

    void analyze_rejectsUnknownAndCorrupted() {
        const fs::path dir = sub("bad");
        std::ofstream(dir / "text.png") << "this is not an image at all, just text";
        auto a = FileAnalyzer(reg_).analyze(dir / "text.png");
        QVERIFY(!a.ok());
        QCOMPARE(a.error().code, ErrorCode::UnsupportedFormat);

        // валидная сигнатура JPEG, но мусор внутри
        {
            std::ofstream f(dir / "broken.jpg", std::ios::binary);
            f << "\xFF\xD8\xFF\xE0" << std::string(200, '\x7F');
        }
        auto b = FileAnalyzer(reg_).analyze(dir / "broken.jpg");
        if (b.ok()) { // если probe пропустил — должна упасть сама конвертация, не оставив выхода
            ConversionSettings s; s.targetFormat = ImageFormat::Png;
            auto r = ConversionEngine(reg_).convert(b.value(), s, CancellationToken(), nullptr);
            QVERIFY(!r.success);
        }
        QVERIFY(!fs::exists(dir / "text.png.png"));
        auto missing = FileAnalyzer(reg_).analyze(dir / "nope.png");
        QVERIFY(!missing.ok());
        QCOMPARE(missing.error().code, ErrorCode::NotFound);
        // выходных и временных файлов не появилось
        QCOMPARE(filesIn(dir).size(), size_t(2));
    }

    void convert_cancelledLeavesNoOutput() {
        const fs::path dir = sub("cancel");
        const fs::path file = makeFile(dir, "c.png", ImageFormat::Png);
        auto info = FileAnalyzer(reg_).analyze(file);
        QVERIFY(info.ok());
        CancellationToken token;
        token.cancel();
        ConversionSettings s; s.targetFormat = ImageFormat::Bmp;
        auto r = ConversionEngine(reg_).convert(info.value(), s, token, nullptr);
        QVERIFY(!r.success);
        QCOMPARE(r.error->code, ErrorCode::Cancelled);
        QCOMPARE(filesIn(dir).size(), size_t(1));
    }

    void convert_conflictPolicies() {
        const fs::path dir = sub("conflict");
        const fs::path file = makeFile(dir, "p.png", ImageFormat::Png);
        auto info = FileAnalyzer(reg_).analyze(file);
        ConversionEngine engine(reg_);
        ConversionSettings s; s.targetFormat = ImageFormat::Bmp;

        auto r1 = engine.convert(info.value(), s, CancellationToken(), nullptr);
        QVERIFY(r1.success);
        QCOMPARE(r1.outputPath.filename(), fs::path("p.bmp"));
        auto r2 = engine.convert(info.value(), s, CancellationToken(), nullptr); // Suffix
        QVERIFY(r2.success);
        QCOMPARE(r2.outputPath.filename(), fs::path("p (1).bmp"));

        s.conflict = ConflictPolicy::Skip;
        auto r3 = engine.convert(info.value(), s, CancellationToken(), nullptr);
        QVERIFY(r3.success && r3.skipped);
        QCOMPARE(filesIn(dir).size(), size_t(3));

        s.conflict = ConflictPolicy::Replace;
        auto r4 = engine.convert(info.value(), s, CancellationToken(), nullptr);
        QVERIFY(r4.success && !r4.skipped);
        QCOMPARE(r4.outputPath.filename(), fs::path("p.bmp"));
        QCOMPARE(filesIn(dir).size(), size_t(3));

        // PNG -> PNG с Replace не должен затирать исходник
        s.targetFormat = ImageFormat::Png;
        const auto before = fs::file_size(file);
        auto r5 = engine.convert(info.value(), s, CancellationToken(), nullptr);
        QVERIFY(r5.success);
        QCOMPARE(r5.outputPath.filename(), fs::path("p (1).png"));
        QCOMPARE(fs::file_size(file), before);
    }

    void convert_exifOrientationAppliedOnce() {
        const fs::path dir = sub("exif");
        const fs::path file = dir / "rot.jpg";
        QImageWriter wr(toQString(file), "jpeg");
        wr.setTransformation(QImageIOHandler::TransformationRotate90);
        QVERIFY(wr.write(makeImage(40, 20, false)));
        QImageReader chk(toQString(file));
        if (chk.transformation() == QImageIOHandler::TransformationNone)
            QSKIP("Плагин JPEG не записал EXIF-ориентацию");

        auto info = FileAnalyzer(reg_).analyze(file);
        QVERIFY(info.ok());
        QCOMPARE(info.value().exifOrientation, 6);
        ConversionSettings s; s.targetFormat = ImageFormat::Png;
        auto r = ConversionEngine(reg_).convert(info.value(), s, CancellationToken(), nullptr);
        QVERIFY2(r.success, r.error ? r.error->message.c_str() : "");
        QImage out(toQString(r.outputPath));
        QCOMPARE(out.width(), 20); // повернуто один раз: 40x20 -> 20x40
        QCOMPARE(out.height(), 40);
    }

    void queue_folderImportDeduplicates() {
        const fs::path dir = sub("queue");
        makeFile(dir, "a.png", ImageFormat::Png);
        makeFile(dir, "b.jpg", ImageFormat::Jpeg);
        std::ofstream(dir / "notes.txt") << "не изображение: папка берёт только известные расширения";

        QueueManager q(reg_);
        QSignalSpy idle(&q, &QueueManager::queueIdle);
        QSignalSpy imported(&q, &QueueManager::importFinished);
        // повторы: тот же файл дважды + папка целиком (содержит тот же файл)
        q.importPaths({dir / "a.png", dir / "a.png", dir});
        QTRY_VERIFY_WITH_TIMEOUT(!idle.isEmpty(), 10000);
        QCOMPARE(imported.size(), 1);
        QCOMPARE(q.tasks().size(), size_t(2)); // a.png и b.jpg; notes.txt отфильтрован
    }

    void queue_endToEnd() {
        const fs::path dir = sub("queue2");
        const fs::path out = root_ / "queue2_out";
        makeFile(dir, "a.png", ImageFormat::Png, true);
        makeFile(dir, "b.jpg", ImageFormat::Jpeg);
        makeFile(dir, "c.bmp", ImageFormat::Bmp);
        std::ofstream(dir / "bad.png") << "not an image";

        QueueManager q(reg_);
        QSignalSpy idle(&q, &QueueManager::queueIdle);
        q.importPaths({dir / "a.png", dir / "a.png", dir / "b.jpg", dir / "c.bmp", dir / "bad.png"});
        QTRY_VERIFY_WITH_TIMEOUT(!idle.isEmpty(), 10000);
        QCOMPARE(q.tasks().size(), size_t(4)); // дубль a.png отброшен

        int ready = 0, rejected = 0;
        for (auto& t : q.tasks()) {
            if (t.state == TaskState::Ready) ++ready;
            if (t.state == TaskState::Rejected) ++rejected;
        }
        QCOMPARE(ready, 3);
        QCOMPARE(rejected, 1);

        ConversionSettings s;
        s.targetFormat = ImageFormat::Jpeg;
        s.outputDir = out;
        idle.clear();
        q.startAllReady(s);
        QTRY_VERIFY_WITH_TIMEOUT(!idle.isEmpty(), 15000);

        int ok = 0;
        for (auto& t : q.tasks()) {
            if (t.state == TaskState::Succeeded) {
                ++ok;
                QVERIFY(fs::exists(t.result.outputPath));
                QCOMPARE(t.progress, 100);
            }
        }
        QCOMPARE(ok, 3);
        QCOMPARE(filesIn(out).size(), size_t(3));

        // Явный повтор успешной задачи: те же настройки, имя с суффиксом (ничего не затёрто)
        const TaskId first = q.tasks().front().id;
        idle.clear();
        q.retry({first}, s);
        QTRY_VERIFY_WITH_TIMEOUT(!idle.isEmpty(), 15000);
        QCOMPARE(filesIn(out).size(), size_t(4));

        // Изменение «настроек UI» после запуска не влияет на снимок
        QVERIFY(q.task(first)->settings.has_value());
        QCOMPARE(q.task(first)->settings->targetFormat, ImageFormat::Jpeg);

        q.clearFinished();
        QCOMPARE(q.tasks().size(), size_t(0));
    }

    void queue_cancelPendingTasks() {
        const fs::path dir = sub("queue3");
        std::vector<fs::path> files;
        for (int i = 0; i < 6; ++i)
            files.push_back(makeFile(dir, QString("big%1.png").arg(i), ImageFormat::Png, false, 1500, 1500));
        QueueManager q(reg_);
        q.setMaxParallel(1);
        QSignalSpy idle(&q, &QueueManager::queueIdle);
        q.importPaths(files);
        QTRY_VERIFY_WITH_TIMEOUT(!idle.isEmpty(), 20000);
        idle.clear();

        ConversionSettings s; s.targetFormat = ImageFormat::Bmp; s.outputDir = root_ / "queue3_out";
        q.startAllReady(s);
        q.cancelAll();
        QTRY_VERIFY_WITH_TIMEOUT(!idle.isEmpty(), 30000);
        int cancelled = 0;
        for (auto& t : q.tasks()) {
            QVERIFY(t.state == TaskState::Cancelled || t.state == TaskState::Succeeded);
            cancelled += t.state == TaskState::Cancelled;
        }
        QVERIFY(cancelled >= 1);
        // у отменённых нет результата и остаточных temp-файлов
        if (fs::exists(root_ / "queue3_out"))
            for (auto& e : fs::directory_iterator(root_ / "queue3_out")) QVERIFY(e.path().extension() != ".tmp");
    }

    void preview_debounceKeepsOnlyLatest() {
        const fs::path dir = sub("preview");
        const fs::path f1 = makeFile(dir, "p1.png", ImageFormat::Png, false, 800, 600);
        const fs::path f2 = makeFile(dir, "p2.png", ImageFormat::Png, false, 400, 300);
        PreviewCache cache;
        PreviewController pc(reg_, cache);
        pc.setDebounceMs(30);
        qRegisterMetaType<std::shared_ptr<const ImageBuffer>>();
        QSignalSpy ready(&pc, &PreviewController::previewReady);

        pc.request(1, f1, 256);
        pc.request(2, f2, 256); // быстрая смена выбора: первый запрос отбрасывается
        QTRY_VERIFY_WITH_TIMEOUT(ready.size() >= 1, 5000);
        QTest::qWait(200);
        QCOMPARE(ready.size(), 1);
        QCOMPARE(ready.first().at(0).toULongLong(), qulonglong(2));
        auto img = ready.first().at(1).value<std::shared_ptr<const ImageBuffer>>();
        QVERIFY(img);
        QVERIFY(img->width <= 256 && img->height <= 256); // scaled read
        QCOMPARE(cache.count(), size_t(1));

        // повторный запрос — из кэша
        ready.clear();
        pc.request(2, f2, 256);
        QTRY_VERIFY_WITH_TIMEOUT(ready.size() == 1, 2000);
    }
};

QTEST_GUILESS_MAIN(IntegrationTests)
#include "test_integration.moc"
