#pragma once
#include "domain/Models.h"

#include "Theme.h"

#include <QDialog>

class QLabel;
class QPushButton;

namespace uic {

class ColorWell;
class SegmentedControl;
class ToggleSwitch;

// Параметры, вынесенные с главного экрана в «Дополнительно».
struct AdvancedOptions {
    fs::path outputDir;                                   // пусто = рядом с исходным файлом
    ConflictPolicy conflict = ConflictPolicy::Suffix;
    AnimationPolicy animation = AnimationPolicy::AllFrames;
    MetadataPolicy metadata = MetadataPolicy::KeepWithoutGps;
    std::uint32_t background = 0xFFFFFF;

    bool operator==(const AdvancedOptions&) const = default;
};

// Лист настроек в стиле iOS: сгруппированные карточки со строками «название — элемент управления».
class AdvancedSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit AdvancedSettingsDialog(const AdvancedOptions& initial, QWidget* parent = nullptr);
    AdvancedOptions options() const;

    // Краткая сводка для подписи под кнопкой «Дополнительно…».
    static QString summary(const AdvancedOptions& o);

protected:
    void paintEvent(QPaintEvent*) override;   // стеклянный фон
    void showEvent(QShowEvent*) override;
    void reject() override;                   // «Отмена» возвращает прежнее оформление

private:
    void load(const AdvancedOptions& o);
    void refresh();
    void chooseFolder();

    ToggleSwitch* sameDir_ = nullptr;
    QWidget* folderRow_ = nullptr;
    QLabel* folderCaption_ = nullptr;
    QPushButton* folderButton_ = nullptr;
    SegmentedControl* conflict_ = nullptr;
    QLabel* conflictCaption_ = nullptr;
    SegmentedControl* animation_ = nullptr;
    QLabel* animationCaption_ = nullptr;
    SegmentedControl* metadata_ = nullptr;
    QLabel* metadataCaption_ = nullptr;
    ColorWell* background_ = nullptr;
    QLabel* backgroundHex_ = nullptr;
    QString folder_;

    SegmentedControl* appearance_ = nullptr;
    ToggleSwitch* glass_ = nullptr;
    Appearance initialAppearance_ = Appearance::System;
    bool initialGlass_ = false;
};

} // namespace uic
