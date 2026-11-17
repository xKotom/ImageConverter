#include "AdvancedSettingsDialog.h"
#include "ColorWell.h"
#include "SegmentedControl.h"
#include "Theme.h"
#include "ToggleSwitch.h"
#include "infrastructure/qt_imageio/QtConvert.h"

#include <QDir>
#include <QPainter>
#include <QScreen>
#include <QScrollArea>
#include <QShowEvent>
#include <QFileDialog>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace uic {

namespace {

QLabel* makeLabel(const QString& text, const char* role) {
    auto* l = new QLabel(text);
    l->setProperty("role", role);
    l->setWordWrap(true);
    return l;
}

QFrame* makeCard() {
    auto* f = new QFrame;
    f->setProperty("card", true);
    auto* v = new QVBoxLayout(f);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    return f;
}

QFrame* hairline() {
    auto* f = new QFrame;
    f->setProperty("hairline", true);
    f->setFixedHeight(1);
    return f;
}

// Строка карточки: слева название и пояснение, справа элемент управления.
QWidget* makeRow(QLabel* title, QLabel* caption, QWidget* control) {
    auto* w = new QWidget;
    w->setObjectName("transparent");
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(16, 10, 16, 10);
    h->setSpacing(16);
    auto* left = new QVBoxLayout;
    left->setSpacing(2);
    left->addWidget(title);
    if (caption) left->addWidget(caption);
    h->addLayout(left, 1);
    h->addWidget(control, 0, Qt::AlignVCenter | Qt::AlignRight);
    w->setMinimumHeight(52);
    return w;
}

} // namespace

AdvancedSettingsDialog::AdvancedSettingsDialog(const AdvancedOptions& initial, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Дополнительные настройки");
    setModal(true);
    setObjectName("glass");
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumWidth(640);
    resize(700, 860);
    initialAppearance_ = Theme::instance().appearance();
    initialGlass_ = Theme::instance().glass();
    connect(&Theme::instance(), &Theme::changed, this, [this] { Theme::instance().applyWindowChrome(this); update(); });

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 20);
    root->setSpacing(10);
    root->addWidget(makeLabel("Дополнительные настройки", "title"));
    auto* sub = makeLabel("Куда сохранять результат и как обращаться с содержимым файлов", "caption");
    root->addWidget(sub);
    root->addSpacing(6);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    scroll->setStyleSheet("QScrollArea { background: transparent; }");
    auto* body = new QWidget;
    body->setObjectName("transparent");
    auto* bv = new QVBoxLayout(body);
    bv->setContentsMargins(0, 0, 6, 0);
    bv->setSpacing(10);

    // ---- Сохранение ----
    bv->addWidget(makeLabel("СОХРАНЕНИЕ", "section"));
    QFrame* save = makeCard();
    auto* sv = static_cast<QVBoxLayout*>(save->layout());

    sameDir_ = new ToggleSwitch;
    sv->addWidget(makeRow(makeLabel("Сохранять рядом с исходным файлом", "row"),
                          makeLabel("Результат появится в той же папке, где лежит оригинал", "caption"), sameDir_));
    sv->addWidget(hairline());

    folderCaption_ = makeLabel({}, "caption");
    folderButton_ = new QPushButton("Выбрать…");
    folderRow_ = makeRow(makeLabel("Папка сохранения", "row"), folderCaption_, folderButton_);
    sv->addWidget(folderRow_);
    sv->addWidget(hairline());

    conflict_ = new SegmentedControl;
    conflict_->addItem("Суффикс", static_cast<int>(ConflictPolicy::Suffix));
    conflict_->addItem("Заменять", static_cast<int>(ConflictPolicy::Replace));
    conflict_->addItem("Пропускать", static_cast<int>(ConflictPolicy::Skip));
    conflictCaption_ = makeLabel({}, "caption");
    sv->addWidget(makeRow(makeLabel("Если файл уже есть", "row"), conflictCaption_, conflict_));
    bv->addWidget(save);

    // ---- Содержимое ----
    bv->addSpacing(6);
    bv->addWidget(makeLabel("СОДЕРЖИМОЕ", "section"));
    QFrame* content = makeCard();
    auto* cv = static_cast<QVBoxLayout*>(content->layout());

    animation_ = new SegmentedControl;
    animation_->addItem("Все кадры", static_cast<int>(AnimationPolicy::AllFrames));
    animation_->addItem("Первый", static_cast<int>(AnimationPolicy::FirstFrameWithWarning));
    animation_->addItem("Отклонять", static_cast<int>(AnimationPolicy::Reject));
    animationCaption_ = makeLabel({}, "caption");
    cv->addWidget(makeRow(makeLabel("Анимация и страницы", "row"), animationCaption_, animation_));
    cv->addWidget(hairline());

    metadata_ = new SegmentedControl;
    metadata_->addItem("Удалить", static_cast<int>(MetadataPolicy::Strip));
    metadata_->addItem("Без GPS", static_cast<int>(MetadataPolicy::KeepWithoutGps));
    metadata_->addItem("Всё", static_cast<int>(MetadataPolicy::KeepAll));
    metadataCaption_ = makeLabel({}, "caption");
    cv->addWidget(makeRow(makeLabel("Метаданные", "row"), metadataCaption_, metadata_));
    cv->addWidget(hairline());

    auto* bgBox = new QWidget;
    bgBox->setObjectName("transparent");
    auto* bgl = new QHBoxLayout(bgBox);
    bgl->setContentsMargins(0, 0, 0, 0);
    bgl->setSpacing(8);
    backgroundHex_ = makeLabel({}, "caption");
    backgroundHex_->setWordWrap(false);
    background_ = new ColorWell;
    bgl->addWidget(backgroundHex_);
    bgl->addWidget(background_);
    cv->addWidget(makeRow(makeLabel("Фон вместо прозрачности", "row"),
                          makeLabel("Заливка для форматов без прозрачности — JPEG и BMP", "caption"), bgBox));
    bv->addWidget(content);

    // ---- Оформление ----
    bv->addSpacing(6);
    bv->addWidget(makeLabel("ОФОРМЛЕНИЕ", "section"));
    QFrame* look = makeCard();
    auto* lv = static_cast<QVBoxLayout*>(look->layout());
    appearance_ = new SegmentedControl;
    appearance_->addItem("Системное", static_cast<int>(Appearance::System));
    appearance_->addItem("Светлое", static_cast<int>(Appearance::Light));
    appearance_->addItem("Тёмное", static_cast<int>(Appearance::Dark));
    lv->addWidget(makeRow(makeLabel("Тема", "row"),
                          makeLabel("Системное — следует за настройкой Windows", "caption"), appearance_));
    lv->addWidget(hairline());
    glass_ = new ToggleSwitch;
    lv->addWidget(makeRow(makeLabel("Стеклянный стиль", "row"),
                          makeLabel("Полупрозрачные панели на цветном фоне, как в iOS и macOS", "caption"), glass_));
    bv->addWidget(look);
    bv->addStretch();
    scroll->setWidget(body);
    root->addWidget(scroll, 1);

    // тема меняется сразу, чтобы результат был виден до нажатия «Готово»
    appearance_->setCurrentData(static_cast<int>(initialAppearance_));
    glass_->setChecked(initialGlass_);
    connect(appearance_, &SegmentedControl::currentIndexChanged, this, [this] {
        Theme::instance().setAppearance(static_cast<Appearance>(appearance_->currentData().toInt()));
    });
    connect(glass_, &QAbstractButton::toggled, this, [](bool on) { Theme::instance().setGlass(on); });

    // ---- Кнопки ----
    root->addSpacing(8);
    auto* buttons = new QHBoxLayout;
    auto* reset = new QPushButton("Сбросить");
    reset->setProperty("link", true);
    reset->setProperty("quiet", true);
    auto* cancel = new QPushButton("Отмена");
    auto* done = new QPushButton("Готово");
    done->setProperty("primary", true);
    done->setDefault(true);
    buttons->addWidget(reset);
    buttons->addStretch();
    buttons->addWidget(cancel);
    buttons->addWidget(done);
    root->addLayout(buttons);

    connect(sameDir_, &QAbstractButton::toggled, this, [this] { refresh(); });
    connect(conflict_, &SegmentedControl::currentIndexChanged, this, [this] { refresh(); });
    connect(animation_, &SegmentedControl::currentIndexChanged, this, [this] { refresh(); });
    connect(metadata_, &SegmentedControl::currentIndexChanged, this, [this] { refresh(); });
    connect(background_, &ColorWell::colorChanged, this, [this] { refresh(); });
    connect(folderButton_, &QPushButton::clicked, this, &AdvancedSettingsDialog::chooseFolder);
    connect(reset, &QPushButton::clicked, this, [this] { load(AdvancedOptions{}); });
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(done, &QPushButton::clicked, this, &QDialog::accept);

    load(initial);
}

void AdvancedSettingsDialog::paintEvent(QPaintEvent* e) {
    Theme::instance().paintBackdrop(this);
    QDialog::paintEvent(e);
}

void AdvancedSettingsDialog::showEvent(QShowEvent* e) {
    QDialog::showEvent(e);
    Theme::instance().applyWindowChrome(this);
    // не выше экрана: остальное прокручивается
    if (screen()) setMaximumHeight(static_cast<int>(screen()->availableGeometry().height() * 0.92));
}

void AdvancedSettingsDialog::reject() {
    Theme::instance().setAppearance(initialAppearance_);
    Theme::instance().setGlass(initialGlass_);
    QDialog::reject();
}

void AdvancedSettingsDialog::load(const AdvancedOptions& o) {
    folder_ = toQString(o.outputDir);
    sameDir_->setChecked(o.outputDir.empty());
    conflict_->setCurrentData(static_cast<int>(o.conflict));
    animation_->setCurrentData(static_cast<int>(o.animation));
    metadata_->setCurrentData(static_cast<int>(o.metadata));
    background_->setColor(QColor::fromRgb(o.background & 0xFFFFFF));
    refresh();
}

void AdvancedSettingsDialog::refresh() {
    const bool same = sameDir_->isChecked();
    folderButton_->setEnabled(!same);
    folderRow_->setEnabled(!same);
    const QFontMetrics fm(folderCaption_->font());
    folderCaption_->setText(same ? "Недоступно: включено сохранение рядом с оригиналом"
                                 : (folder_.isEmpty() ? QString("Папка не выбрана")
                                                      : fm.elidedText(QDir::toNativeSeparators(folder_), Qt::ElideMiddle, 330)));
    folderCaption_->setToolTip(same ? QString() : QDir::toNativeSeparators(folder_));

    switch (static_cast<ConflictPolicy>(conflict_->currentData().toInt())) {
    case ConflictPolicy::Suffix: conflictCaption_->setText("К имени добавляется номер: фото (1).png"); break;
    case ConflictPolicy::Replace: conflictCaption_->setText("Существующий результат заменяется — перед запуском будет вопрос"); break;
    case ConflictPolicy::Skip: conflictCaption_->setText("Файл с таким именем остаётся нетронутым, исходный пропускается"); break;
    }
    switch (static_cast<AnimationPolicy>(animation_->currentData().toInt())) {
    case AnimationPolicy::AllFrames: animationCaption_->setText("Сохранять все кадры, если формат результата это умеет"); break;
    case AnimationPolicy::FirstFrameWithWarning: animationCaption_->setText("Сохранять только первый кадр, с предупреждением"); break;
    case AnimationPolicy::Reject: animationCaption_->setText("Не обрабатывать файлы с несколькими кадрами"); break;
    }
    switch (static_cast<MetadataPolicy>(metadata_->currentData().toInt())) {
    case MetadataPolicy::Strip: metadataCaption_->setText("EXIF, XMP и IPTC в результат не попадают"); break;
    case MetadataPolicy::KeepWithoutGps: metadataCaption_->setText("Камера, дата и прочее сохраняются; координаты GPS удаляются"); break;
    case MetadataPolicy::KeepAll: metadataCaption_->setText("Переносится всё, включая координаты места съёмки"); break;
    }
    backgroundHex_->setText(background_->color().name().toUpper());
}

void AdvancedSettingsDialog::chooseFolder() {
    const QString d = QFileDialog::getExistingDirectory(this, "Папка сохранения", folder_);
    if (!d.isEmpty()) { folder_ = d; refresh(); }
}

AdvancedOptions AdvancedSettingsDialog::options() const {
    AdvancedOptions o;
    if (!sameDir_->isChecked() && !folder_.isEmpty()) o.outputDir = toPath(folder_);
    o.conflict = static_cast<ConflictPolicy>(conflict_->currentData().toInt());
    o.animation = static_cast<AnimationPolicy>(animation_->currentData().toInt());
    o.metadata = static_cast<MetadataPolicy>(metadata_->currentData().toInt());
    o.background = background_->color().rgb() & 0xFFFFFF;
    return o;
}

QString AdvancedSettingsDialog::summary(const AdvancedOptions& o) {
    QStringList parts;
    parts << (o.outputDir.empty() ? QString("рядом с оригиналом") : toQString(o.outputDir.filename()));
    switch (o.conflict) {
    case ConflictPolicy::Suffix: parts << "суффикс"; break;
    case ConflictPolicy::Replace: parts << "замена"; break;
    case ConflictPolicy::Skip: parts << "пропуск"; break;
    }
    switch (o.animation) {
    case AnimationPolicy::AllFrames: parts << "все кадры"; break;
    case AnimationPolicy::FirstFrameWithWarning: parts << "первый кадр"; break;
    case AnimationPolicy::Reject: parts << "без анимации"; break;
    }
    switch (o.metadata) {
    case MetadataPolicy::Strip: parts << "без метаданных"; break;
    case MetadataPolicy::KeepWithoutGps: parts << "без GPS"; break;
    case MetadataPolicy::KeepAll: parts << "с GPS"; break;
    }
    return parts.join(" · ");
}

} // namespace uic
