#pragma once
#include "AdvancedSettingsDialog.h"
#include "application/CommandLineHandler.h"
#include "application/PreviewController.h"
#include "application/QueueManager.h"
#include "infrastructure/settings/SettingsManager.h"

#include <QMainWindow>
#include <QPixmap>

class QComboBox;
class QLabel;
class QListView;
class QProgressBar;
class QPushButton;
class QSlider;
class QStackedWidget;
class QTextBrowser;

namespace uic {

class DropArea;
class FileListModel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(CodecRegistry& registry, QueueManager& queue, PreviewController& preview,
               SettingsManager& settings, QWidget* parent = nullptr);

    // Применяет аргументы командной строки (позже — payload Shell-команды).
    void applyCommandLine(const CommandLineOptions& opts);

protected:
    void closeEvent(QCloseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private slots:
    void addFiles();
    void addFolder();
    void importPaths(const QStringList& paths);
    void startConversion();
    void cancelSelectedOrAll();
    void retrySelected();
    void removeSelected();
    void onSelectionChanged();
    void onQueueChanged();
    void onSettingsChanged();
    void openAdvanced();
    void showCapabilities();
    void showContextMenu(const QPoint& pos);
    void onPreviewReady(uic::TaskId id, std::shared_ptr<const uic::ImageBuffer> image);
    void onPreviewFailed(uic::TaskId id, const QString& message);

private:
    void buildUi();
    void applyIcons();
    ConversionSettings currentSettings() const;
    void applySettingsToUi(const ConversionSettings& s);
    std::vector<TaskId> selectedIds() const;
    void refreshDetails();
    void updateButtons();
    void updateQualityControls();
    void updateAdvancedSummary();
    void updatePreviewPixmap();
    void setPreviewText(const QString& text);
    void openResult(TaskId id, bool showInFolder);

    CodecRegistry& registry_;
    QueueManager& queue_;
    PreviewController& preview_;
    SettingsManager& settings_;

    FileListModel* model_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    DropArea* dropArea_ = nullptr;
    QListView* list_ = nullptr;
    QLabel* countLabel_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* folderButton_ = nullptr;
    QPushButton* clearButton_ = nullptr;

    QComboBox* formatCombo_ = nullptr;
    QSlider* qualitySlider_ = nullptr;
    QLabel* qualityValue_ = nullptr;
    QLabel* qualityCaption_ = nullptr;
    QPushButton* advancedButton_ = nullptr;
    QLabel* advancedSummary_ = nullptr;

    QLabel* previewLabel_ = nullptr;
    QTextBrowser* details_ = nullptr;
    QPushButton* convertButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
    QPushButton* retryButton_ = nullptr;

    QWidget* footer_ = nullptr;
    QProgressBar* overall_ = nullptr;
    QLabel* overallLabel_ = nullptr;

    AdvancedOptions adv_;
    QPixmap previewPixmap_;
    TaskId previewId_ = 0;
    bool loading_ = false; // подавляет реакцию на программное заполнение настроек
};

} // namespace uic
