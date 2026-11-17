#include "application/CodecRegistry.h"
#include "application/CommandLineHandler.h"
#include "application/PreviewCache.h"
#include "application/PreviewController.h"
#include "application/QueueManager.h"
#include "application/CapabilityReport.h"
#include "infrastructure/codecs/Codecs.h"
#include "infrastructure/settings/SettingsManager.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QIcon>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
// Приложение оконное (без консоли); для CLI-режимов подключаемся к консоли родителя.
void printToConsole(const QString& text) {
#ifdef _WIN32
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        SetConsoleOutputCP(CP_UTF8);
        std::freopen("CONOUT$", "w", stdout);
        std::freopen("CONOUT$", "w", stderr);
    }
#endif
    std::fputs(text.toUtf8().constData(), stdout);
    std::fputs("\n", stdout);
    std::fflush(stdout);
}
} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("ImageConverter");
    QApplication::setOrganizationName("ImageConverter");
    QApplication::setStyle("Fusion");

    QIcon appIcon;
    for (const char* f : {":/icons/app-32.png", ":/icons/app-64.png", ":/icons/app-256.png", ":/icons/app-512.png"})
        appIcon.addFile(f);
    QApplication::setWindowIcon(appIcon);

    uic::SettingsManager settings;
    // Стиль в духе macOS/iOS: светлая/тёмная схема и необязательное «стекло» (настройки → «Дополнительно…»)
    uic::Theme::instance().apply(app, static_cast<uic::Appearance>(settings.appearance()), settings.glass());

    // Корень композиции: здесь выбираются реализации портов.
    uic::CodecRegistry registry;
    uic::registerAllCodecs(registry);

    const uic::CommandLineOptions opts = uic::CommandLineHandler::parse(app.arguments());
    if (!opts.error.isEmpty()) { printToConsole("Ошибка аргументов: " + opts.error); return 2; }
    if (opts.listCodecs) {
        printToConsole(QString::fromStdString(uic::CapabilityReport::summaryLine(registry) + "\n\n" +
                                              uic::CapabilityReport::detailedText(registry)));
        return 0;
    }

    uic::QueueManager queue(registry);
    uic::PreviewCache cache;
    uic::PreviewController preview(registry, cache);

    uic::MainWindow window(registry, queue, preview, settings);
    window.show();
    window.applyCommandLine(opts);
    return app.exec();
}
