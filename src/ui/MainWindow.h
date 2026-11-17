#pragma once
#include "application/CommandLineHandler.h"
#include "application/PreviewController.h"
#include "application/QueueManager.h"
#include "infrastructure/settings/SettingsManager.h"

#include <QMainWindow>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSlider;
class QSpinBox;
class QTableView;
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
    void chooseOutputDir();
    void chooseBackground();
    void showContextMenu(const QPoint& pos);
    void onPreviewReady(uic::TaskId id, std::shared_ptr<const uic::ImageBuffer> image);
    void onPreviewFailed(uic::TaskId id, const QString& message);

private:
    void buildUi();
    ConversionSettings currentSettings() const;
    void applySettingsToUi(const ConversionSettings& s);
    std::vector<TaskId> selectedIds() const;
    void refreshDetails();
    void updateButtons();
    void updateQualityControls();
    void openResult(TaskId id, bool showInFolder);

    CodecRegistry& registry_;
    QueueManager& queue_;
    PreviewController& preview_;
    SettingsManager& settings_;

    FileListModel* model_ = nullptr;
    DropArea* dropArea_ = nullptr;
    QTableView* table_ = nullptr;
    QComboBox* formatCombo_ = nullptr;
    QSlider* qualitySlider_ = nullptr;
    QSpinBox* qualitySpin_ = nullptr;
    QCheckBox* sameDirCheck_ = nullptr;
    QLineEdit* outDirEdit_ = nullptr;
    QPushButton* outDirButton_ = nullptr;
    QComboBox* conflictCombo_ = nullptr;
    QCheckBox* rejectAnimCheck_ = nullptr;
    QPushButton* bgButton_ = nullptr;
    QLabel* previewLabel_ = nullptr;
    QTextBrowser* details_ = nullptr;
    QPushButton* convertButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
    QPushButton* retryButton_ = nullptr;
    QPushButton* removeButton_ = nullptr;
    QPushButton* clearButton_ = nullptr;
    QProgressBar* overall_ = nullptr;
    QLabel* statusLabel_ = nullptr;

    std::uint32_t background_ = 0xFFFFFF;
    TaskId previewId_ = 0;
    bool loading_ = false; // подавляет реакцию на программное заполнение настроек
};

} // namespace uic
