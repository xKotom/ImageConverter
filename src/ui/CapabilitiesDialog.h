#pragma once
#include "application/CodecRegistry.h"

#include <QDialog>

namespace uic {

// Окно «Поддерживаемые форматы»: что реально доступно в этой сборке и какие есть ограничения.
class CapabilitiesDialog : public QDialog {
    Q_OBJECT
public:
    explicit CapabilitiesDialog(const CodecRegistry& registry, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent*) override;
    void showEvent(QShowEvent*) override;
};

} // namespace uic
