#include "game_settings_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QSaveFile>
#include <QSpinBox>
#include <QVBoxLayout>

GameSettingsDialog::GameSettingsDialog(const QString& path, QWidget* parent) : QDialog(parent), m_path(path) {
    setObjectName("gameSettingsDialog");
    setWindowTitle(tr("Paramètres du jeu"));

    QFile f(m_path);
    if (f.open(QIODevice::ReadOnly)) m_settings.loadFromString(f.readAll().toStdString());

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    layout->addLayout(form);

    auto* fullscreen = new QCheckBox;
    fullscreen->setChecked(m_settings.fullscreen);
    form->addRow(tr("Plein écran"), fullscreen);

    auto* vsync = new QCheckBox;
    vsync->setChecked(m_settings.vsync);
    form->addRow(tr("VSync"), vsync);

    auto* aa = new QComboBox;
    for (unsigned level : { 0u, 2u, 4u, 8u, 16u })
        aa->addItem(level ? QString("%1x").arg(level) : tr("Désactivé"), level);
    aa->setCurrentIndex(qMax(0, aa->findData(m_settings.antialiasing)));
    form->addRow(tr("Antialiasing"), aa);

    auto* fps = new QSpinBox;
    fps->setRange(0, 1000);
    fps->setSpecialValueText(tr("Illimité"));   // 0
    fps->setValue(int(m_settings.fpsLimit));
    fps->setEnabled(!m_settings.vsync);
    fps->setToolTip(tr("Ignoré avec la VSync"));
    form->addRow(tr("FPS max"), fps);

    m_status = new QLabel;
    m_status->setObjectName("hintLabel");
    layout->addWidget(m_status);

    connect(fullscreen, &QCheckBox::toggled, this, [this](bool on) { m_settings.fullscreen = on; save(); });
    connect(vsync, &QCheckBox::toggled, this, [this, fps](bool on) {
        m_settings.vsync = on;
        fps->setEnabled(!on);
        save();
    });
    connect(aa, &QComboBox::currentIndexChanged, this, [this, aa] {
        m_settings.antialiasing = aa->currentData().toUInt();
        save();
    });
    connect(fps, &QSpinBox::valueChanged, this, [this](int v) { m_settings.fpsLimit = unsigned(v); save(); });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void GameSettingsDialog::save() {
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile out(m_path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)
        || out.write(QByteArray::fromStdString(m_settings.toString())) < 0 || !out.commit()) {
        m_status->setText(tr("Impossible d'enregistrer %1").arg(QDir::toNativeSeparators(m_path)));
        return;
    }
    m_status->setText(tr("Enregistré"));
}
