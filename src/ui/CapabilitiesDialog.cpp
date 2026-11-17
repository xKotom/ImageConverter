#include "CapabilitiesDialog.h"
#include "application/CapabilityReport.h"
#include "infrastructure/qt_imageio/QtConvert.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

namespace uic {

CapabilitiesDialog::CapabilitiesDialog(const CodecRegistry& registry, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Поддерживаемые форматы");
    resize(980, 420);
    auto* lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel(
        "Поддержка определяется при запуске по реально загруженным кодекам, а не по списку расширений. "
        "«Анимация/страницы» — сохранение всех кадров; «Метаданные» — перенос EXIF/XMP/IPTC в результат."));
    static_cast<QLabel*>(lay->itemAt(0)->widget())->setWordWrap(true);

    const auto rows = CapabilityReport::rows(registry);
    auto* t = new QTableWidget(static_cast<int>(rows.size()), 8);
    t->setHorizontalHeaderLabels({"Формат", "Чтение", "Запись", "Анимация: чтение", "Анимация: запись",
                                  "Метаданные: запись", "Прозрачность", "Примечание"});
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->verticalHeader()->hide();
    t->setWordWrap(true);

    auto mark = [](bool b) {
        auto* it = new QTableWidgetItem(b ? "✔" : "—");
        it->setTextAlignment(Qt::AlignCenter);
        it->setForeground(b ? QColor(0x1b, 0x7f, 0x3b) : QColor(0x99, 0x99, 0x99));
        return it;
    };
    int r = 0;
    for (const auto& row : rows) {
        t->setItem(r, 0, new QTableWidgetItem(QString::fromUtf8(row.label)));
        t->setItem(r, 1, mark(row.cap.read));
        t->setItem(r, 2, mark(row.cap.write));
        t->setItem(r, 3, mark(row.cap.readSequence));
        t->setItem(r, 4, mark(row.cap.writeSequence));
        t->setItem(r, 5, mark(row.cap.writeMetadata));
        t->setItem(r, 6, mark(row.alpha));
        t->setItem(r, 7, new QTableWidgetItem(fromUtf8(row.notes)));
        ++r;
    }
    t->resizeColumnsToContents();
    t->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Stretch);
    t->resizeRowsToContents();
    lay->addWidget(t, 1);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(box);
}

} // namespace uic
