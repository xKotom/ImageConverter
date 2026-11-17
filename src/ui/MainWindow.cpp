#include "MainWindow.h"
#include "DropArea.h"
#include "FileListModel.h"
#include "infrastructure/qt_imageio/QtCodecs.h"
#include "infrastructure/qt_imageio/QtConvert.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTableView>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace uic {

namespace {
QString colorStyle(std::uint32_t rgb) {
    return QString("background-color:#%1;").arg(rgb & 0xFFFFFF, 6, 16, QChar('0'));
}
} // namespace

MainWindow::MainWindow(CodecRegistry& registry, QueueManager& queue, PreviewController& preview,
                       SettingsManager& settings, QWidget* parent)
    : QMainWindow(parent), registry_(registry), queue_(queue), preview_(preview), settings_(settings) {
    setWindowTitle("Универсальный конвертер изображений");
    setAcceptDrops(true);
    resize(1180, 720);
    buildUi();

    connect(&queue_, &QueueManager::taskChanged, this, [this](TaskId id) {
        onQueueChanged();
        if (id == previewId_) refreshDetails();
    });
    connect(&queue_, &QueueManager::taskAdded, this, [this] { onQueueChanged(); });
    connect(&queue_, &QueueManager::taskRemoved, this, [this] { onQueueChanged(); onSelectionChanged(); });
    connect(&queue_, &QueueManager::queueIdle, this, [this] { updateButtons(); });
    connect(&queue_, &QueueManager::importFinished, this, [this](int added, int dups) {
        statusBar()->showMessage(QString("Добавлено файлов: %1, повторов пропущено: %2").arg(added).arg(dups), 5000);
    });
    connect(&preview_, &PreviewController::previewReady, this, &MainWindow::onPreviewReady);
    connect(&preview_, &PreviewController::previewFailed, this, &MainWindow::onPreviewFailed);

    applySettingsToUi(settings_.loadConversion());
    queue_.setMaxParallel(settings_.maxParallel());
    if (auto g = settings_.windowGeometry(); !g.isEmpty()) restoreGeometry(g);
    if (auto s = settings_.windowState(); !s.isEmpty()) restoreState(s);
    onQueueChanged();
    updateButtons();
}

void MainWindow::buildUi() {
    auto* central = new QWidget;
    setCentralWidget(central);
    auto* root = new QHBoxLayout(central);
    auto* split = new QSplitter(Qt::Horizontal);
    root->addWidget(split);

    // ---- левая часть: список ----
    auto* left = new QWidget;
    auto* lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 0, 0);

    dropArea_ = new DropArea;
    connect(dropArea_, &DropArea::pathsDropped, this, &MainWindow::importPaths);
    lv->addWidget(dropArea_);

    auto* bar = new QHBoxLayout;
    auto* addBtn = new QPushButton("Добавить файлы…");
    auto* addDirBtn = new QPushButton("Добавить папку…");
    removeButton_ = new QPushButton("Удалить");
    clearButton_ = new QPushButton("Очистить завершённые");
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::addFiles);
    connect(addDirBtn, &QPushButton::clicked, this, &MainWindow::addFolder);
    connect(removeButton_, &QPushButton::clicked, this, &MainWindow::removeSelected);
    connect(clearButton_, &QPushButton::clicked, this, [this] { queue_.clearFinished(); });
    bar->addWidget(addBtn);
    bar->addWidget(addDirBtn);
    bar->addStretch();
    bar->addWidget(removeButton_);
    bar->addWidget(clearButton_);
    lv->addLayout(bar);

    model_ = new FileListModel(queue_, this);
    table_ = new QTableView;
    table_->setModel(model_);
    table_->setItemDelegate(new ProgressDelegate(table_));
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->setWordWrap(false);
    table_->verticalHeader()->hide();
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setColumnWidth(FileListModel::ColFile, 220);
    table_->setColumnWidth(FileListModel::ColFormat, 110);
    table_->setColumnWidth(FileListModel::ColSize, 150);
    table_->setColumnWidth(FileListModel::ColStatus, 150);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QWidget::customContextMenuRequested, this, &MainWindow::showContextMenu);
    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::onSelectionChanged);
    connect(table_, &QTableView::doubleClicked, this, [this](const QModelIndex& i) {
        openResult(model_->idAt(i.row()), false);
    });
    lv->addWidget(table_, 1);

    overall_ = new QProgressBar;
    overall_->setTextVisible(true);
    overall_->hide();
    lv->addWidget(overall_);
    split->addWidget(left);

    // ---- правая часть: настройки + предпросмотр ----
    auto* right = new QWidget;
    auto* rv = new QVBoxLayout(right);
    rv->setContentsMargins(0, 0, 0, 0);

    auto* sg = new QGroupBox("Параметры конвертации");
    auto* form = new QVBoxLayout(sg);

    formatCombo_ = new QComboBox;
    for (ImageFormat f : registry_.writableFormats())
        formatCombo_->addItem(QString::fromUtf8(traits(f).label), static_cast<int>(f));
    auto* fl = new QHBoxLayout;
    fl->addWidget(new QLabel("Формат:"));
    fl->addWidget(formatCombo_, 1);
    form->addLayout(fl);

    qualitySlider_ = new QSlider(Qt::Horizontal);
    qualitySlider_->setRange(1, 100);
    qualitySpin_ = new QSpinBox;
    qualitySpin_->setRange(1, 100);
    qualitySlider_->setToolTip("Шкала зависит от кодировщика. Для WEBP значение 100 включает режим без потерь.");
    auto* ql = new QHBoxLayout;
    ql->addWidget(new QLabel("Качество:"));
    ql->addWidget(qualitySlider_, 1);
    ql->addWidget(qualitySpin_);
    form->addLayout(ql);
    connect(qualitySlider_, &QSlider::valueChanged, qualitySpin_, &QSpinBox::setValue);
    connect(qualitySpin_, &QSpinBox::valueChanged, qualitySlider_, &QSlider::setValue);

    sameDirCheck_ = new QCheckBox("Сохранять рядом с исходным файлом");
    form->addWidget(sameDirCheck_);
    outDirEdit_ = new QLineEdit;
    outDirEdit_->setPlaceholderText("Папка результата");
    outDirButton_ = new QPushButton("…");
    outDirButton_->setMaximumWidth(32);
    auto* dl = new QHBoxLayout;
    dl->addWidget(outDirEdit_, 1);
    dl->addWidget(outDirButton_);
    form->addLayout(dl);
    connect(outDirButton_, &QPushButton::clicked, this, &MainWindow::chooseOutputDir);

    conflictCombo_ = new QComboBox;
    conflictCombo_->addItem("Добавлять суффикс (1), (2)…", static_cast<int>(ConflictPolicy::Suffix));
    conflictCombo_->addItem("Заменять существующий (с подтверждением)", static_cast<int>(ConflictPolicy::Replace));
    conflictCombo_->addItem("Пропускать", static_cast<int>(ConflictPolicy::Skip));
    auto* cl = new QHBoxLayout;
    cl->addWidget(new QLabel("Если файл уже есть:"));
    cl->addWidget(conflictCombo_, 1);
    form->addLayout(cl);

    rejectAnimCheck_ = new QCheckBox("Отклонять анимированные файлы (иначе — только первый кадр)");
    form->addWidget(rejectAnimCheck_);

    bgButton_ = new QPushButton;
    bgButton_->setFixedWidth(60);
    bgButton_->setToolTip("Цвет, которым заливается прозрачность при сохранении в формат без alpha (JPEG, BMP)");
    auto* bl = new QHBoxLayout;
    bl->addWidget(new QLabel("Фон вместо прозрачности:"));
    bl->addWidget(bgButton_);
    bl->addStretch();
    form->addLayout(bl);
    connect(bgButton_, &QPushButton::clicked, this, &MainWindow::chooseBackground);

    for (auto* w : {static_cast<QWidget*>(formatCombo_), static_cast<QWidget*>(conflictCombo_)})
        connect(static_cast<QComboBox*>(w), &QComboBox::currentIndexChanged, this, &MainWindow::onSettingsChanged);
    connect(qualitySpin_, &QSpinBox::valueChanged, this, &MainWindow::onSettingsChanged);
    connect(sameDirCheck_, &QCheckBox::toggled, this, &MainWindow::onSettingsChanged);
    connect(outDirEdit_, &QLineEdit::textChanged, this, &MainWindow::onSettingsChanged);
    connect(rejectAnimCheck_, &QCheckBox::toggled, this, &MainWindow::onSettingsChanged);
    rv->addWidget(sg);

    auto* pg = new QGroupBox("Предпросмотр");
    auto* pv = new QVBoxLayout(pg);
    previewLabel_ = new QLabel("Выберите файл в списке");
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setMinimumSize(320, 220);
    previewLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    previewLabel_->setStyleSheet("background:#2b2b2b;color:#bbb;border-radius:6px;");
    pv->addWidget(previewLabel_, 3);
    details_ = new QTextBrowser;
    details_->setMaximumHeight(190);
    pv->addWidget(details_, 2);
    rv->addWidget(pg, 1);

    auto* actions = new QHBoxLayout;
    convertButton_ = new QPushButton("Конвертировать");
    convertButton_->setDefault(true);
    cancelButton_ = new QPushButton("Отмена");
    retryButton_ = new QPushButton("Повторить");
    actions->addWidget(convertButton_, 2);
    actions->addWidget(cancelButton_);
    actions->addWidget(retryButton_);
    rv->addLayout(actions);
    connect(convertButton_, &QPushButton::clicked, this, &MainWindow::startConversion);
    connect(cancelButton_, &QPushButton::clicked, this, &MainWindow::cancelSelectedOrAll);
    connect(retryButton_, &QPushButton::clicked, this, &MainWindow::retrySelected);

    split->addWidget(right);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);

    statusLabel_ = new QLabel(QString::fromStdString(describeCapabilities(registry_)));
    statusBar()->addPermanentWidget(statusLabel_);
}

// ---------- настройки ----------

ConversionSettings MainWindow::currentSettings() const {
    ConversionSettings s;
    s.targetFormat = static_cast<ImageFormat>(formatCombo_->currentData().toInt());
    s.quality = qualitySpin_->value();
    if (!sameDirCheck_->isChecked()) s.outputDir = toPath(outDirEdit_->text().trimmed());
    s.conflict = static_cast<ConflictPolicy>(conflictCombo_->currentData().toInt());
    s.animation = rejectAnimCheck_->isChecked() ? AnimationPolicy::Reject : AnimationPolicy::FirstFrameWithWarning;
    s.backgroundRgb = background_;
    return s;
}

void MainWindow::applySettingsToUi(const ConversionSettings& s) {
    loading_ = true;
    int idx = formatCombo_->findData(static_cast<int>(s.targetFormat));
    formatCombo_->setCurrentIndex(idx >= 0 ? idx : 0);
    qualitySpin_->setValue(s.quality);
    sameDirCheck_->setChecked(s.outputDir.empty());
    outDirEdit_->setText(toQString(s.outputDir));
    conflictCombo_->setCurrentIndex(std::max(0, conflictCombo_->findData(static_cast<int>(s.conflict))));
    rejectAnimCheck_->setChecked(s.animation == AnimationPolicy::Reject);
    background_ = s.backgroundRgb;
    bgButton_->setStyleSheet(colorStyle(background_));
    loading_ = false;
    updateQualityControls();
}

void MainWindow::updateQualityControls() {
    const auto f = static_cast<ImageFormat>(formatCombo_->currentData().toInt());
    const bool lossy = traits(f).lossy;
    qualitySlider_->setEnabled(lossy);
    qualitySpin_->setEnabled(lossy);
    const bool same = sameDirCheck_->isChecked();
    outDirEdit_->setEnabled(!same);
    outDirButton_->setEnabled(!same);
    bgButton_->setEnabled(!traits(f).supportsAlpha);
}

void MainWindow::onSettingsChanged() {
    if (loading_) return;
    updateQualityControls();
    settings_.saveConversion(currentSettings());
    refreshDetails(); // предупреждения зависят от выбранных настроек
    updateButtons();
}

void MainWindow::chooseOutputDir() {
    const QString d = QFileDialog::getExistingDirectory(this, "Папка результата", outDirEdit_->text());
    if (!d.isEmpty()) { outDirEdit_->setText(d); sameDirCheck_->setChecked(false); }
}

void MainWindow::chooseBackground() {
    const QColor c = QColorDialog::getColor(QColor::fromRgb(background_), this, "Фон вместо прозрачности");
    if (!c.isValid()) return;
    background_ = c.rgb() & 0xFFFFFF;
    bgButton_->setStyleSheet(colorStyle(background_));
    onSettingsChanged();
}

// ---------- импорт ----------

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

// ---------- управление очередью ----------

std::vector<TaskId> MainWindow::selectedIds() const {
    std::vector<TaskId> ids;
    for (const QModelIndex& i : table_->selectionModel()->selectedRows()) ids.push_back(model_->idAt(i.row()));
    return ids;
}

void MainWindow::startConversion() {
    const ConversionSettings s = currentSettings();
    if (!s.outputDir.empty() && !sameDirCheck_->isChecked() && outDirEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Папка результата", "Укажите папку результата или включите сохранение рядом с исходником.");
        return;
    }
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
    const QModelIndex i = table_->indexAt(pos);
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
    QAction* a = m.exec(table_->viewport()->mapToGlobal(pos));
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

// ---------- предпросмотр и подробности ----------

void MainWindow::onSelectionChanged() {
    const auto ids = selectedIds();
    if (ids.size() != 1) {
        previewId_ = 0;
        preview_.cancel();
        previewLabel_->setPixmap({});
        previewLabel_->setText(ids.empty() ? "Выберите файл в списке" : "Выбрано несколько файлов");
        refreshDetails();
        updateButtons();
        return;
    }
    previewId_ = ids.front();
    const ConversionTask* t = queue_.task(previewId_);
    previewLabel_->setPixmap({});
    previewLabel_->setText("Загрузка…");
    if (t && t->info) preview_.request(previewId_, t->source);
    else previewLabel_->setText("Предпросмотр недоступен");
    refreshDetails();
    updateButtons();
}

void MainWindow::onPreviewReady(TaskId id, std::shared_ptr<const ImageBuffer> image) {
    if (id != previewId_ || !image) return; // результат для уже сменившейся строки
    const QPixmap px = QPixmap::fromImage(toQImage(*image));
    previewLabel_->setText({});
    previewLabel_->setPixmap(px.scaled(previewLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::onPreviewFailed(TaskId id, const QString& message) {
    if (id != previewId_) return;
    previewLabel_->setPixmap({});
    previewLabel_->setText("Предпросмотр недоступен:\n" + message);
}

void MainWindow::refreshDetails() {
    const ConversionTask* t = previewId_ ? queue_.task(previewId_) : nullptr;
    if (!t) { details_->clear(); return; }
    QString h = "<b>" + toQString(t->source.filename()).toHtmlEscaped() + "</b><br>";
    h += "<span style='color:gray'>" + toQString(t->source.parent_path()).toHtmlEscaped() + "</span><br>";
    if (t->info) {
        const FileInfo& i = *t->info;
        h += QString("Формат (по содержимому): <b>%1</b>; расширение: .%2<br>")
                 .arg(traits(i.actualFormat).label, QString::fromStdString(i.extensionHint));
        h += QString("Размер: %1×%2; кадров: %3; прозрачность: %4; EXIF-ориентация: %5<br>")
                 .arg(i.width).arg(i.height).arg(i.frameCount).arg(i.hasAlpha ? "да" : "нет").arg(i.exifOrientation);
        const ConversionPlan plan = queue_.planner().plan(i, currentSettings());
        if (!plan.allowed) h += "<span style='color:#c02b2b'>✖ " + fromUtf8(plan.rejectReason).toHtmlEscaped() + "</span><br>";
        for (const auto& w : plan.warnings) h += "⚠ " + fromUtf8(w).toHtmlEscaped() + "<br>";
    }
    if (!t->message.empty() && (t->state == TaskState::Failed || t->state == TaskState::Rejected))
        h += "<span style='color:#c02b2b'>" + fromUtf8(t->message).toHtmlEscaped() + "</span><br>";
    if (t->state == TaskState::Succeeded)
        h += "<span style='color:#1b7f3b'>Результат: " + toQString(t->result.outputPath).toHtmlEscaped() +
             QString(" (%1 мс)").arg(t->result.durationMs) + "</span>";
    details_->setHtml(h);
}

// ---------- состояние кнопок и общий прогресс ----------

void MainWindow::onQueueChanged() {
    double done = 0;
    int total = 0, finished = 0;
    for (const auto& t : queue_.tasks()) {
        if (!t.settings || t.state == TaskState::Ready || t.state == TaskState::Analyzing) continue;
        ++total;
        switch (t.state) {
        case TaskState::Queued: case TaskState::Running: case TaskState::Cancelling: done += t.progress / 100.0; break;
        case TaskState::Rejected: break;
        default: done += 1; ++finished; break;
        }
    }
    overall_->setVisible(total > 0);
    if (total > 0) {
        overall_->setRange(0, total * 100);
        overall_->setValue(static_cast<int>(done * 100));
        overall_->setFormat(QString("Обработано %1 из %2").arg(finished).arg(total));
    }
    updateButtons();
}

void MainWindow::updateButtons() {
    int ready = 0, active = 0, retryable = 0;
    for (const auto& t : queue_.tasks()) {
        if (t.state == TaskState::Ready) ++ready;
        if (t.state == TaskState::Queued || t.state == TaskState::Running) ++active;
        if (t.state == TaskState::Failed || t.state == TaskState::Cancelled) ++retryable;
    }
    const bool hasSel = !selectedIds().empty();
    convertButton_->setEnabled(ready > 0);
    cancelButton_->setEnabled(active > 0);
    retryButton_->setEnabled(retryable > 0 || hasSel);
    removeButton_->setEnabled(hasSel);
    clearButton_->setEnabled(!queue_.tasks().empty());
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
    settings_.setWindowState(saveState());
    e->accept();
}

} // namespace uic
