#include "game_settings_dialog.h"
#include "render/post_process.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kSliderSteps = 1000;

std::vector<PostProcess::Param> paramsOf(const QString& fragPath) {
    QFile f(fragPath);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return PostProcess::parseParams(f.readAll().toStdString());
}
} // namespace

GameSettingsDialog::GameSettingsDialog(const QString& path, const QString& projectDir, QWidget* parent)
    : QDialog(parent), m_path(path), m_projectDir(projectDir) {
    setObjectName("gameSettingsDialog");
    setWindowTitle(tr("Paramètres du jeu"));
    resize(440, 620);

    QFile f(m_path);
    if (f.open(QIODevice::ReadOnly)) m_settings.loadFromString(f.readAll().toStdString());

    auto* layout = new QVBoxLayout(this);

    // ---- fenêtre
    auto* windowBox = new QGroupBox(tr("Fenêtre"));
    auto* form = new QFormLayout(windowBox);
    layout->addWidget(windowBox);

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

    // ---- post-process
    auto* postBox = new QGroupBox(tr("Post-process"));
    auto* postLayout = new QVBoxLayout(postBox);
    layout->addWidget(postBox, 1);

    auto* shaderRow = new QHBoxLayout;
    m_shader = new QLineEdit(QString::fromStdString(m_settings.shader));
    m_shader->setPlaceholderText(tr("Aucun"));
    auto* browse = new QToolButton;
    browse->setText("...");
    browse->setToolTip(tr("Choisir un .frag"));
    auto* reset = new QPushButton(tr("Réinitialiser"));
    reset->setToolTip(tr("Valeurs par défaut"));
    shaderRow->addWidget(m_shader, 1);
    shaderRow->addWidget(browse);
    shaderRow->addWidget(reset);
    postLayout->addLayout(shaderRow);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* paramsWidget = new QWidget;
    m_params = new QFormLayout(paramsWidget);
    scroll->setWidget(paramsWidget);
    postLayout->addWidget(scroll, 1);

    connect(m_shader, &QLineEdit::editingFinished, this, [this] { setShader(m_shader->text().trimmed()); });
    connect(browse, &QToolButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(this, tr("Shader de post-process"), m_projectDir,
                                                          tr("Fragment shader (*.frag)"));
        if (file.isEmpty()) return;
        // relatif au projet quand il est dedans
        QString relative = QDir(m_projectDir).relativeFilePath(file);
        if (relative.startsWith("..")) relative = file;
        m_shader->setText(relative);
        setShader(relative);
    });
    connect(reset, &QPushButton::clicked, this, [this] {
        m_settings.shaderParams.clear();
        rebuildParams();
        save();
    });

    m_status = new QLabel;
    m_status->setObjectName("hintLabel");
    layout->addWidget(m_status);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    rebuildParams();
}

void GameSettingsDialog::setShader(const QString& relative) {
    if (relative.toStdString() == m_settings.shader) return;
    m_settings.shader = relative.toStdString();
    m_settings.shaderParams.clear();   // réglages de l'ancien shader
    rebuildParams();
    save();
}

void GameSettingsDialog::rebuildParams() {
    while (m_params->rowCount() > 0) m_params->removeRow(0);
    if (m_settings.shader.empty()) return;

    const QString file = QDir(m_projectDir).absoluteFilePath(QString::fromStdString(m_settings.shader));
    std::map<std::string, float> kept;   // seulement les réglages que ce shader connaît
    for (const PostProcess::Param& p : paramsOf(file)) {
        const auto it = m_settings.shaderParams.find(p.name);
        const float value = it != m_settings.shaderParams.end() ? it->second : p.value;
        kept[p.name] = value;

        auto* slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, kSliderSteps);
        auto* spin = new QDoubleSpinBox;
        spin->setRange(p.min, p.max);
        spin->setDecimals(3);
        spin->setSingleStep((p.max - p.min) / 100.0);
        spin->setValue(value);
        spin->setToolTip(tr("Défaut : %1").arg(p.value));
        const auto toSlider = [p](double v) { return qRound((v - p.min) / (p.max - p.min) * kSliderSteps); };
        slider->setValue(toSlider(value));

        connect(slider, &QSlider::valueChanged, spin, [spin, p](int pos) {
            spin->setValue(p.min + (p.max - p.min) * pos / double(kSliderSteps));
        });
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this, slider, toSlider, name = p.name](double v) {
            const QSignalBlocker block(slider);
            slider->setValue(toSlider(v));
            m_settings.shaderParams[name] = float(v);
            save();
        });

        auto* row = new QWidget;
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->addWidget(slider, 1);
        h->addWidget(spin);
        m_params->addRow(QString::fromStdString(p.name), row);
    }
    m_settings.shaderParams = kept;
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
    emit saved(m_settings);
}
