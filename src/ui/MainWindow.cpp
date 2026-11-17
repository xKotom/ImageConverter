#include "MainWindow.h"
#include "AdvancedSettingsDialog.h"
#include "CapabilitiesDialog.h"
#include "DropArea.h"
#include "FileListModel.h"
#include "Icons.h"
#include "Theme.h"
#include "application/CapabilityReport.h"
#include "infrastructure/qt_imageio/QtConvert.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QFontMetrics>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QProgressBar>
#include <QShowEvent>
#include <QPushButton>
#include <QShortcut>
#include <QSlider>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>
#include <set>

namespace uic {

namespace {

QLabel* makeLabel(const QString& text, const char* role) {
    auto* l = new QLabel(text);
    l->setProperty("role", role);
    return l;
}

QFrame* makeCard() {
    auto* f = new QFrame;
    f->setProperty("card", true);
    return f;
}

QFrame* hairline() {
    auto* f = new QFrame;
    f->setProperty("hairline", true);
    f->setFixedHeight(1);
    return f;
}

QString plural(int n, const char* one, const char* few, const char* many) {
    const int m10 = n % 10, m100 = n % 100;
    const char* w = (m10 == 1 && m100 != 11) ? one : (m10 >= 2 && m10 <= 4 && (m100 < 10 || m100 >= 20)) ? few : many;
    return QString::fromUtf8(w);
}

QPixmap rounded(const QPixmap& src, int radius) {
    QPixmap out(src.size());
    out.setDevicePixelRatio(src.devicePixelRatio());
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    const QSizeF logical = QSizeF(src.size()) / src.devicePixelRatio();
    path.addRoundedRect(QRectF(QPointF(0, 0), logical), radius, radius);
    p.setClipPath(path);
    p.drawPixmap(0, 0, src);
    return out;
}

} // namespace

MainWindow::MainWindow(CodecRegistry& registry, QueueManager& queue, PreviewController& preview,
                       SettingsManager& settings, QWidget* parent)
    : QMainWindow(parent), registry_(registry), queue_(queue), preview_(preview), settings_(settings) {
    setWindowTitle("ImageConverter");
    setObjectName("glass");
    setAttribute(Qt::WA_TranslucentBackground); // нужно для размытия фона в стеклянном стиле
    setAcceptDrops(true);
    setMinimumSize(1000, 720);
    resize(1180, 780);
    buildUi();

    connect(&queue_, &QueueManager::taskChanged, this, [this](TaskId id) {
        onQueueChanged();
        if (id == previewId_) refreshDetails();
    });
    connect(&queue_, &QueueManager::taskAdded, this, [this] { onQueueChanged(); });
    connect(&queue_, &QueueManager::taskRemoved, this, [this] { onQueueChanged(); onSelectionChanged(); });
    connect(&queue_, &QueueManager::queueIdle, this, [this] { updateButtons(); });
    connect(&queue_, &QueueManager::importFinished, this, [this](int added, int dups) {
        statusBar()->showMessage(QString("Добавлено: %1 · повторов пропущено: %2").arg(added).arg(dups), 5000);
    });
    connect(&preview_, &PreviewController::previewReady, this, &MainWindow::onPreviewReady);
    connect(&preview_, &PreviewController::previewFailed, this, &MainWindow::onPreviewFailed);
    connect(&Theme::instance(), &Theme::changed, this, [this] {
        applyIcons();
        Theme::instance().applyWindowChrome(this);
        update();
        list_->viewport()->update();
        refreshDetails();
    });

    applySettingsToUi(settings_.loadConversion());
    queue_.setMaxParallel(settings_.maxParallel());
    if (auto g = settings_.windowGeometry(); !g.isEmpty()) restoreGeometry(g);
    onQueueChanged();
    updateButtons();
}

// ================== построение интерфейса ==================

void MainWindow::buildUi() {
    auto* central = new QWidget;
    setCentralWidget(central);
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(24, 20, 24, 14);
    root->setSpacing(22);

    // ---------- левая колонка: заголовок, очередь ----------
    auto* left = new QVBoxLayout;
    left->setSpacing(14);

    auto* header = new QHBoxLayout;
    header->setSpacing(8);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(0);
    titles->addWidget(makeLabel("Очередь", "title"));
    countLabel_ = makeLabel("Нет файлов", "caption");
    titles->addWidget(countLabel_);
    header->addLayout(titles);
    header->addStretch();
    clearButton_ = new QPushButton("Очистить завершённые");
    clearButton_->setProperty("link", true);
    clearButton_->setProperty("quiet", true);
    folderButton_ = new QPushButton("Папка…");
    addButton_ = new QPushButton("Добавить");
    for (auto* b : {clearButton_, folderButton_, addButton_}) b->setCursor(Qt::PointingHandCursor);
    header->addWidget(clearButton_);
    header->addWidget(folderButton_);
    header->addWidget(addButton_);
    left->addLayout(header);

    dropArea_ = new DropArea; // живёт в правой панели (см. ниже)

    // Предпросмотр: крупная картинка слева, подробности и предупреждения справа.
    auto* pvCard = makeCard();
    auto* pv = new QVBoxLayout(pvCard);
    pv->setContentsMargins(14, 14, 14, 12);
    pv->setSpacing(10);
    previewLabel_ = new QLabel;
    previewLabel_->setProperty("role", "preview");
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setMinimumSize(260, 140);
    previewLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    pv->addWidget(previewLabel_, 1);
    details_ = new QTextBrowser;
    details_->setFixedHeight(84);
    details_->setOpenLinks(false);
    pv->addWidget(details_, 0);
    left->addWidget(pvCard, 2);

    // Очередь под предпросмотром: пустое состояние или лента файлов.
    stack_ = new QStackedWidget;
    auto* emptyCard = makeCard();
    auto* ecl = new QVBoxLayout(emptyCard);
    auto* emptyText = makeLabel("Очередь пуста — перетащите изображения в область справа", "caption");
    emptyText->setAlignment(Qt::AlignCenter);
    ecl->addWidget(emptyText);
    stack_->addWidget(emptyCard);

    auto* listCard = makeCard();
    auto* lcl = new QVBoxLayout(listCard);
    lcl->setContentsMargins(4, 6, 4, 6);
    model_ = new FileListModel(queue_, this);
    list_ = new QListView;
    list_->setModel(model_);
    list_->setItemDelegate(new RowDelegate(list_));
    list_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    list_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    list_->setUniformItemSizes(true);
    list_->setMouseTracking(true);
    list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_->setContextMenuPolicy(Qt::CustomContextMenu);
    list_->setFrameShape(QFrame::NoFrame);
    lcl->addWidget(list_);
    stack_->addWidget(listCard);
    left->addWidget(stack_, 3);

    footer_ = new QWidget;
    footer_->setObjectName("transparent");
    auto* fl = new QHBoxLayout(footer_);
    fl->setContentsMargins(2, 0, 2, 0);
    overallLabel_ = makeLabel({}, "caption");
    overall_ = new QProgressBar;
    overall_->setTextVisible(false);
    fl->addWidget(overallLabel_);
    fl->addWidget(overall_, 1);
    footer_->hide();
    left->addWidget(footer_);
    root->addLayout(left, 1);

    // ---------- правая колонка: результат, предпросмотр, действия ----------
    auto* side = new QWidget;
    side->setObjectName("transparent");
    side->setFixedWidth(348);
    auto* sv = new QVBoxLayout(side);
    sv->setContentsMargins(0, 0, 0, 0);
    sv->setSpacing(8);

    sv->addWidget(makeLabel("РЕЗУЛЬТАТ", "section"));
    auto* fmtCard = makeCard();
    auto* fv = new QVBoxLayout(fmtCard);
    fv->setContentsMargins(16, 10, 16, 12);
    fv->setSpacing(8);
    auto* fmtRow = new QHBoxLayout;
    fmtRow->addWidget(makeLabel("Формат", "row"));
    fmtRow->addStretch();
    formatCombo_ = new QComboBox;
    formatCombo_->setMinimumWidth(150);
    formatCombo_->setCursor(Qt::PointingHandCursor);
    for (const auto& row : CapabilityReport::rows(registry_)) {
        if (!row.cap.write) continue; // формат без кодировщика не предлагается
        formatCombo_->addItem(QString::fromUtf8(row.label), static_cast<int>(row.cap.format));
        formatCombo_->setItemData(formatCombo_->count() - 1, fromUtf8(CapabilityReport::tooltipFor(row)), Qt::ToolTipRole);
    }
    fmtRow->addWidget(formatCombo_);
    fv->addLayout(fmtRow);
    fv->addWidget(hairline());
    auto* qRow = new QHBoxLayout;
    qRow->addWidget(makeLabel("Качество", "row"));
    qRow->addStretch();
    qualityValue_ = makeLabel("90%", "row");
    qRow->addWidget(qualityValue_);
    fv->addLayout(qRow);
    qualitySlider_ = new QSlider(Qt::Horizontal);
    qualitySlider_->setRange(1, 100);
    qualitySlider_->setCursor(Qt::PointingHandCursor);
    qualitySlider_->setToolTip("Шкала зависит от кодировщика. Для WEBP значение 100 включает режим без потерь.");
    fv->addWidget(qualitySlider_);
    qualityCaption_ = makeLabel({}, "caption");
    qualityCaption_->setWordWrap(true);
    fv->addWidget(qualityCaption_);
    sv->addWidget(fmtCard);

    sv->addSpacing(4);
    sv->addWidget(dropArea_, 1);

    advancedButton_ = new QPushButton("  Дополнительно…");
    advancedButton_->setCursor(Qt::PointingHandCursor);
    sv->addWidget(advancedButton_);
    advancedSummary_ = makeLabel({}, "caption");
    advancedSummary_->setWordWrap(true);
    advancedSummary_->setAlignment(Qt::AlignHCenter);
    sv->addWidget(advancedSummary_);

    convertButton_ = new QPushButton("Конвертировать");
    convertButton_->setProperty("primary", true);
    convertButton_->setCursor(Qt::PointingHandCursor);
    convertButton_->setDefault(true);
    sv->addWidget(convertButton_);
    auto* act = new QHBoxLayout;
    act->setSpacing(8);
    cancelButton_ = new QPushButton("Отмена");
    retryButton_ = new QPushButton("Повторить");
    for (auto* b : {cancelButton_, retryButton_}) { b->setCursor(Qt::PointingHandCursor); act->addWidget(b, 1); }
    sv->addLayout(act);
    auto* caps = new QPushButton("Поддерживаемые форматы");
    caps->setProperty("link", true);
    caps->setProperty("quiet", true);
    caps->setCursor(Qt::PointingHandCursor);
    caps->setToolTip(fromUtf8(CapabilityReport::summaryLine(registry_)));
    sv->addWidget(caps, 0, Qt::AlignHCenter);
    root->addWidget(side);

    statusBar()->setSizeGripEnabled(false);

    // ---------- связи ----------
    connect(dropArea_, &DropArea::pathsDropped, this, &MainWindow::importPaths);
    connect(dropArea_, &DropArea::addFilesRequested, this, &MainWindow::addFiles);
    connect(dropArea_, &DropArea::addFolderRequested, this, &MainWindow::addFolder);
    connect(addButton_, &QPushButton::clicked, this, &MainWindow::addFiles);
    connect(folderButton_, &QPushButton::clicked, this, &MainWindow::addFolder);
    connect(clearButton_, &QPushButton::clicked, this, [this] { queue_.clearFinished(); });
    connect(list_, &QWidget::customContextMenuRequested, this, &MainWindow::showContextMenu);
    connect(list_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::onSelectionChanged);
    connect(list_, &QListView::doubleClicked, this, [this](const QModelIndex& i) { openResult(model_->idAt(i.row()), false); });
    auto* del = new QShortcut(QKeySequence::Delete, list_);
    del->setContext(Qt::WidgetShortcut);
    connect(del, &QShortcut::activated, this, &MainWindow::removeSelected);

    connect(formatCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::onSettingsChanged);
    connect(qualitySlider_, &QSlider::valueChanged, this, [this](int v) {
        qualityValue_->setText(QString("%1%").arg(v));
        onSettingsChanged();
    });
    connect(advancedButton_, &QPushButton::clicked, this, &MainWindow::openAdvanced);
    connect(convertButton_, &QPushButton::clicked, this, &MainWindow::startConversion);
    connect(cancelButton_, &QPushButton::clicked, this, &MainWindow::cancelSelectedOrAll);
    connect(retryButton_, &QPushButton::clicked, this, &MainWindow::retrySelected);
    connect(caps, &QPushButton::clicked, this, &MainWindow::showCapabilities);

    applyIcons();
    setPreviewText("Выберите файл в списке");
}

void MainWindow::applyIcons() {
    const ThemeColors& c = Theme::instance().colors();
    addButton_->setIcon(Icons::icon(Icons::Kind::Plus, 16, c.text));
    folderButton_->setIcon(Icons::icon(Icons::Kind::Folder, 16, c.text));
    advancedButton_->setIcon(Icons::icon(Icons::Kind::Sliders, 17, c.text));
    for (auto* b : {addButton_, folderButton_, advancedButton_}) b->setIconSize({16, 16});
}

// ================== настройки ==================

ConversionSettings MainWindow::currentSettings() const {
    ConversionSettings s;
    s.targetFormat = static_cast<ImageFormat>(formatCombo_->currentData().toInt());
    s.quality = qualitySlider_->value();
    s.outputDir = adv_.outputDir;
    s.conflict = adv_.conflict;
    s.animation = adv_.animation;
    s.metadata = adv_.metadata;
    s.backgroundRgb = adv_.background;
    return s;
}

void MainWindow::applySettingsToUi(const ConversionSettings& s) {
    loading_ = true;
    const int idx = formatCombo_->findData(static_cast<int>(s.targetFormat));
    formatCombo_->setCurrentIndex(idx >= 0 ? idx : 0);
    qualitySlider_->setValue(s.quality);
    qualityValue_->setText(QString("%1%").arg(s.quality));
    adv_.outputDir = s.outputDir;
    adv_.conflict = s.conflict;
    adv_.animation = s.animation;
    adv_.metadata = s.metadata;
    adv_.background = s.backgroundRgb;
    loading_ = false;
    updateQualityControls();
    updateAdvancedSummary();
}

void MainWindow::updateQualityControls() {
    const auto f = static_cast<ImageFormat>(formatCombo_->currentData().toInt());
    const bool lossy = traits(f).lossy;
    qualitySlider_->setEnabled(lossy);
    qualityValue_->setEnabled(lossy);
    qualityCaption_->setText(lossy ? (f == ImageFormat::Webp || f == ImageFormat::Avif
                                          ? "100% включает режим без потерь"
                                          : "Чем выше значение, тем больше размер файла")
                                   : "Без потерь — качество не применяется");
}

void MainWindow::updateAdvancedSummary() {
    advancedSummary_->setText(AdvancedSettingsDialog::summary(adv_));
}

void MainWindow::onSettingsChanged() {
    if (loading_) return;
    updateQualityControls();
    settings_.saveConversion(currentSettings());
    refreshDetails(); // предупреждения зависят от выбранных настроек
    updateButtons();
}

void MainWindow::openAdvanced() {
    AdvancedSettingsDialog dlg(adv_, this);
    if (dlg.exec() != QDialog::Accepted) return;
    // оформление применялось в окне сразу; при «Готово» запоминаем его
    settings_.setAppearance(static_cast<int>(Theme::instance().appearance()));
    settings_.setGlass(Theme::instance().glass());
    adv_ = dlg.options();
    updateAdvancedSummary();
    settings_.saveConversion(currentSettings());
    refreshDetails();
}

void MainWindow::showCapabilities() {
    CapabilitiesDialog dlg(registry_, this);
    dlg.exec();
}

// ================== импорт ==================

void MainWindow::addFiles() {
    QStringList exts;
    for (ImageFormat f : registry_.readableFormats())
        for (const auto& e : traits(f).extensions) exts << "*." + QString::fromStdString(e);
    const QString filter = "Изображения (" + exts.join(' ') + ");;Все файлы (*)";
    const QStringList files = QFileDialog::getOpenFileNames(this, "Добавить изображения", settings_.lastDirectory(), filter);
    if (files.isEmpty()) return;
    settings_.setLastDirectory(QFileInfo(files.first()).absolutePath());
    importPaths(files);
}

void MainWindow::addFolder() {
    const QString d = QFileDialog::getExistingDirectory(this, "Добавить папку", settings_.lastDirectory());
    if (d.isEmpty()) return;
    settings_.setLastDirectory(d);
    importPaths({d});
}

void MainWindow::importPaths(const QStringList& paths) {
    std::vector<fs::path> v;
    for (const auto& p : paths) v.push_back(toPath(p));
    queue_.importPaths(v);
    statusBar()->showMessage("Импорт…", 2000);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e) {
    if (!DropArea::pathsFromMime(e->mimeData()).isEmpty()) e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* e) {
    const QStringList p = DropArea::pathsFromMime(e->mimeData());
    if (!p.isEmpty()) { e->acceptProposedAction(); importPaths(p); }
}

void MainWindow::applyCommandLine(const CommandLineOptions& o) {
    ConversionSettings s = currentSettings();
    if (o.format && registry_.canWrite(*o.format)) s.targetFormat = *o.format;
    if (o.quality) s.quality = *o.quality;
    if (o.outputDir) s.outputDir = *o.outputDir;
    applySettingsToUi(s);
    if (!o.paths.empty()) queue_.importPaths(o.paths);
    if (o.autoStart) {
        // Дожидаемся окончания импорта и анализа, затем запускаем.
        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(&queue_, &QueueManager::queueIdle, this, [this, conn] {
            disconnect(*conn);
            startConversion();
        });
    }
}

// ================== управление очередью ==================

std::vector<TaskId> MainWindow::selectedIds() const {
    std::vector<TaskId> ids;
    std::set<int> rows;
    for (const QModelIndex& i : list_->selectionModel()->selectedIndexes()) rows.insert(i.row());
    for (int r : rows) ids.push_back(model_->idAt(r));
    return ids;
}

void MainWindow::startConversion() {
    const ConversionSettings s = currentSettings();
    if (s.conflict == ConflictPolicy::Replace &&
        QMessageBox::warning(this, "Замена файлов",
            "Существующие файлы с совпадающими именами будут заменены. Исходные изображения затронуты не будут.\n\nПродолжить?",
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    settings_.saveConversion(s);
    queue_.startAllReady(s);
    updateButtons();
}

void MainWindow::cancelSelectedOrAll() {
    auto ids = selectedIds();
    if (ids.empty()) queue_.cancelAll();
    else for (TaskId id : ids) queue_.cancel(id);
}

void MainWindow::retrySelected() {
    auto ids = selectedIds();
    if (ids.empty())
        for (const auto& t : queue_.tasks())
            if (t.state == TaskState::Failed || t.state == TaskState::Cancelled) ids.push_back(t.id);
    queue_.retry(ids, currentSettings());
}

void MainWindow::removeSelected() {
    for (TaskId id : selectedIds()) queue_.remove(id);
}

void MainWindow::showContextMenu(const QPoint& pos) {
    const QModelIndex i = list_->indexAt(pos);
    if (!i.isValid()) return;
    const TaskId id = model_->idAt(i.row());
    const ConversionTask* t = queue_.task(id);
    if (!t) return;
    QMenu m;
    auto* open = m.addAction("Открыть результат");
    auto* show = m.addAction("Показать в папке");
    open->setEnabled(t->state == TaskState::Succeeded);
    show->setEnabled(t->state == TaskState::Succeeded);
    m.addSeparator();
    auto* retry = m.addAction("Повторить");
    auto* cancel = m.addAction("Отменить");
    auto* remove = m.addAction("Удалить из списка");
    QAction* a = m.exec(list_->viewport()->mapToGlobal(pos));
    if (a == open) openResult(id, false);
    else if (a == show) openResult(id, true);
    else if (a == retry) queue_.retry({id}, currentSettings());
    else if (a == cancel) queue_.cancel(id);
    else if (a == remove) queue_.remove(id);
}

void MainWindow::openResult(TaskId id, bool showInFolder) {
    const ConversionTask* t = queue_.task(id);
    if (!t || t->state != TaskState::Succeeded) return;
    const QString path = toQString(t->result.outputPath);
    if (showInFolder) {
#ifdef Q_OS_WIN
        QProcess::startDetached("explorer.exe", {"/select,", QDir::toNativeSeparators(path)});
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

// ================== предпросмотр и подробности ==================

void MainWindow::setPreviewText(const QString& text) {
    previewPixmap_ = QPixmap();
    previewLabel_->setPixmap(QPixmap());
    previewLabel_->setText(text);
}

void MainWindow::updatePreviewPixmap() {
    if (previewPixmap_.isNull()) return;
    const QSize target = previewLabel_->size() - QSize(2, 2);
    QPixmap scaled = previewPixmap_.scaled(target * previewLabel_->devicePixelRatioF(), Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(previewLabel_->devicePixelRatioF());
    previewLabel_->setPixmap(rounded(scaled, 8));
}

void MainWindow::paintEvent(QPaintEvent* e) {
    Theme::instance().paintBackdrop(this);
    QMainWindow::paintEvent(e);
}

void MainWindow::showEvent(QShowEvent* e) {
    QMainWindow::showEvent(e);
    Theme::instance().applyWindowChrome(this);
}

void MainWindow::resizeEvent(QResizeEvent* e) {
    QMainWindow::resizeEvent(e);
    updatePreviewPixmap();
}

void MainWindow::onSelectionChanged() {
    const auto ids = selectedIds();
    if (ids.size() != 1) {
        previewId_ = 0;
        preview_.cancel();
        setPreviewText(ids.empty() ? "Выберите файл в списке" : "Выбрано несколько файлов");
        refreshDetails();
        updateButtons();
        return;
    }
    previewId_ = ids.front();
    const ConversionTask* t = queue_.task(previewId_);
    setPreviewText("Загрузка…");
    if (t && t->info) preview_.request(previewId_, t->source);
    else setPreviewText("Предпросмотр недоступен");
    refreshDetails();
    updateButtons();
}

void MainWindow::onPreviewReady(TaskId id, std::shared_ptr<const ImageBuffer> image) {
    if (id != previewId_ || !image) return; // результат для уже сменившейся строки
    previewPixmap_ = QPixmap::fromImage(toQImage(*image));
    previewLabel_->setText({});
    updatePreviewPixmap();
}

void MainWindow::onPreviewFailed(TaskId id, const QString& message) {
    if (id != previewId_) return;
    setPreviewText("Предпросмотр недоступен\n" + message);
}

void MainWindow::refreshDetails() {
    const ConversionTask* t = previewId_ ? queue_.task(previewId_) : nullptr;
    if (!t) { details_->clear(); return; }
    const ThemeColors& c = Theme::instance().colors();
    const QString dim = c.text2.name(), warn = c.warning.name(), bad = c.danger.name(), good = c.success.name();
    QString h = QString("<div style='font-size:14px;font-weight:600'>%1</div>").arg(toQString(t->source.filename()).toHtmlEscaped());
    const QFontMetrics fm(details_->font());
    const QString dir = fm.elidedText(QDir::toNativeSeparators(toQString(t->source.parent_path())), Qt::ElideMiddle,
                                      std::max(120, details_->width() - 40));
    h += QString("<div style='color:%1;font-size:12px'>%2</div><br>").arg(dim, dir.toHtmlEscaped());
    if (t->info) {
        const FileInfo& i = *t->info;
        h += QString("<div style='color:%1;font-size:12px'>%2 · %3×%4 · %5 кадр. · прозрачность: %6</div>")
                 .arg(dim, traits(i.actualFormat).label).arg(i.width).arg(i.height).arg(i.frameCount)
                 .arg(i.hasAlpha ? "да" : "нет");
        if (i.metadata.any())
            h += QString("<div style='color:%1;font-size:12px'>Метаданные: %2%3%4%5</div>")
                     .arg(dim, i.metadata.hasExif ? "EXIF " : "", i.metadata.hasXmp ? "XMP " : "",
                          i.metadata.hasIptc ? "IPTC " : "", i.metadata.hasGps ? "· <b>есть GPS</b>" : "");
        const ConversionPlan plan = queue_.planner().plan(i, currentSettings());
        h += "<br>";
        if (!plan.allowed)
            h += QString("<div style='color:%1'>✖ %2</div>").arg(bad, fromUtf8(plan.rejectReason).toHtmlEscaped());
        for (const auto& w : plan.warnings)
            h += QString("<div style='font-size:12px'><span style='color:%1'>●</span> %2</div>").arg(warn, fromUtf8(w).toHtmlEscaped());
    }
    if (!t->message.empty() && (t->state == TaskState::Failed || t->state == TaskState::Rejected))
        h += QString("<br><div style='color:%1'>%2</div>").arg(bad, fromUtf8(t->message).toHtmlEscaped());
    if (t->state == TaskState::Succeeded)
        h += QString("<br><div style='color:%1'>Готово за %2 мс<br>%3</div>")
                 .arg(good).arg(t->result.durationMs).arg(toQString(t->result.outputPath).toHtmlEscaped());
    details_->setHtml(h);
}

// ================== состояние кнопок и общий прогресс ==================

void MainWindow::onQueueChanged() {
    const int count = static_cast<int>(queue_.tasks().size());
    stack_->setCurrentIndex(count == 0 ? 0 : 1);

    double done = 0;
    int total = 0, finished = 0, ready = 0;
    for (const auto& t : queue_.tasks()) {
        if (t.state == TaskState::Ready) ++ready;
        if (!t.settings || t.state == TaskState::Ready || t.state == TaskState::Analyzing) continue;
        ++total;
        switch (t.state) {
        case TaskState::Queued: case TaskState::Running: case TaskState::Cancelling: done += t.progress / 100.0; break;
        case TaskState::Rejected: break;
        default: done += 1; ++finished; break;
        }
    }
    countLabel_->setText(count == 0 ? QString("Нет файлов")
                                    : QString("%1 %2 · %3 готово к конвертации").arg(count)
                                          .arg(plural(count, "файл", "файла", "файлов")).arg(ready));
    footer_->setVisible(total > 0);
    if (total > 0) {
        overall_->setRange(0, total * 100);
        overall_->setValue(static_cast<int>(done * 100));
        overallLabel_->setText(QString("Обработано %1 из %2").arg(finished).arg(total));
    }
    updateButtons();
}

void MainWindow::updateButtons() {
    int ready = 0, active = 0, retryable = 0, finished = 0;
    for (const auto& t : queue_.tasks()) {
        if (t.state == TaskState::Ready) ++ready;
        if (t.state == TaskState::Queued || t.state == TaskState::Running) ++active;
        if (t.state == TaskState::Failed || t.state == TaskState::Cancelled) ++retryable;
        if (t.state == TaskState::Succeeded || t.state == TaskState::Failed || t.state == TaskState::Cancelled ||
            t.state == TaskState::Skipped || t.state == TaskState::Rejected) ++finished;
    }
    const bool hasSel = !selectedIds().empty();
    convertButton_->setEnabled(ready > 0);
    cancelButton_->setEnabled(active > 0);
    retryButton_->setEnabled(retryable > 0 || hasSel);
    clearButton_->setVisible(finished > 0); // показываем, только когда есть что очищать
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (queue_.isBusy()) {
        if (QMessageBox::question(this, "Выход", "Есть незавершённые задачи. Отменить их и выйти?",
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
            e->ignore();
            return;
        }
        queue_.cancelAll();
    }
    settings_.saveConversion(currentSettings());
    settings_.setWindowGeometry(saveGeometry());
    e->accept();
}

} // namespace uic
