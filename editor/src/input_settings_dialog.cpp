#include "input_settings_dialog.h"
#include "qt_sfml_input.h"

#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QSaveFile>
#include <QSpinBox>
#include <QSplitter>
#include <QStyle>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

using Source = InputBinding::Source;

QString sourceLabel(Source s) {
    switch (s) {
    case Source::Key:            return QObject::tr("Touche");
    case Source::MouseButton:    return QObject::tr("Bouton souris");
    case Source::MouseWheel:     return QObject::tr("Molette");
    case Source::JoystickButton: return QObject::tr("Bouton manette");
    case Source::JoystickAxis:   return QObject::tr("Axe manette");
    }
    return {};
}

const Source kSources[] = { Source::Key, Source::MouseButton, Source::MouseWheel, Source::JoystickButton, Source::JoystickAxis };

QString keyLabel(sf::Keyboard::Key k) {
    return k == sf::Keyboard::Key::Unknown ? QObject::tr("(aucune)") : QString::fromStdString(InputManager::keyName(k));
}

} // namespace

// ---- KeyCaptureButton --------------------------------------------------------------------

KeyCaptureButton::KeyCaptureButton(sf::Keyboard::Key key, QWidget* parent) : QPushButton(parent), m_key(key) {
    setText(keyLabel(key));
    setToolTip(tr("Cliquer, puis appuyer sur la touche voulue (Échap : annuler)"));
    setFocusPolicy(Qt::StrongFocus);
    connect(this, &QPushButton::clicked, this, [this] { setCapturing(!m_capturing); });
}

void KeyCaptureButton::setCapturing(bool on) {
    m_capturing = on;
    setText(on ? tr("Appuyez sur une touche…") : keyLabel(m_key));
    setProperty("capturing", on);   // QSS : QPushButton[capturing="true"]
    style()->unpolish(this);
    style()->polish(this);
    if (on) setFocus();
}

bool KeyCaptureButton::event(QEvent* e) {
    if (m_capturing) {
        // Les raccourcis de l'éditeur (F5, Ctrl+S...) ne doivent pas partir pendant la capture
        if (e->type() == QEvent::ShortcutOverride) { e->accept(); return true; }
        if (e->type() == QEvent::KeyPress) {
            auto* k = static_cast<QKeyEvent*>(e);
            if (k->key() == Qt::Key_Escape) { setCapturing(false); return true; }
            const sf::Keyboard::Key key = QtSfml::toSfmlKey(k);
            if (key != sf::Keyboard::Key::Unknown) {
                m_key = key;
                setCapturing(false);
                emit keyCaptured(key);
            }
            return true;
        }
    }
    return QPushButton::event(e);
}

void KeyCaptureButton::focusOutEvent(QFocusEvent* e) {
    if (m_capturing) setCapturing(false);
    QPushButton::focusOutEvent(e);
}

// ---- InputSettingsDialog -----------------------------------------------------------------

InputSettingsDialog::InputSettingsDialog(const QString& configPath, std::function<void(InputManager&)> defaults,
                                         QWidget* parent)
    : QDialog(parent), m_path(configPath), m_defaults(std::move(defaults)) {
    setWindowTitle(tr("Input Manager"));
    setObjectName("inputSettingsDialog");
    resize(820, 480);

    // Fichier du projet, sinon les actions par défaut du projet
    bool fromFile = false;
    QFile f(m_path);
    if (f.open(QIODevice::ReadOnly)) fromFile = m_input.loadFromString(f.readAll().toStdString());
    if (!fromFile && m_defaults) m_defaults(m_input);

    auto* root = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("Le jeu lit ces actions par leur nom : <code>Input::getAxis(\"Horizontal\")</code>, "
                                "<code>Input::getButtonDown(\"Jump\")</code>."));
    intro->setObjectName("hintLabel");
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto* split = new QSplitter(Qt::Horizontal);
    root->addWidget(split, 1);

    // --- Gauche : actions
    auto* left = new QWidget;
    auto* lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 0, 0);
    m_list = new QListWidget;
    lv->addWidget(m_list, 1);
    auto* lb = new QHBoxLayout;
    auto* addAction = new QPushButton(tr("+ Action"));
    auto* delAction = new QPushButton(tr("Supprimer"));
    lb->addWidget(addAction);
    lb->addWidget(delAction);
    lv->addLayout(lb);
    split->addWidget(left);

    // --- Droite : détails de l'action
    m_details = new QWidget;
    auto* rv = new QVBoxLayout(m_details);
    rv->setContentsMargins(6, 0, 0, 0);
    auto* form = new QFormLayout;
    m_name = new QLineEdit;
    m_type = new QComboBox;
    m_type->addItems({ tr("Bouton (enfoncé / relâché)"), tr("Axe (valeur de -1 à 1)") });
    m_deadZone = new QDoubleSpinBox;
    m_deadZone->setRange(0.0, 0.99);
    m_deadZone->setSingleStep(0.05);
    m_deadZone->setToolTip(tr("Manette : en dessous de cette valeur, le stick compte pour 0"));
    form->addRow(tr("Nom"), m_name);
    form->addRow(tr("Type"), m_type);
    form->addRow(tr("Zone morte"), m_deadZone);
    rv->addLayout(form);

    m_bindings = new QTableWidget(0, 5);
    m_bindings->setHorizontalHeaderLabels({ tr("Source"), tr("Entrée"), tr("Échelle"), tr("Manette"), QString() });
    m_bindings->verticalHeader()->setVisible(false);
    m_bindings->setSelectionMode(QAbstractItemView::NoSelection);
    m_bindings->setFocusPolicy(Qt::NoFocus);
    m_bindings->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_bindings->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_bindings->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_bindings->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_bindings->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_bindings->verticalHeader()->setDefaultSectionSize(30);
    rv->addWidget(m_bindings, 1);
    auto* addBinding = new QPushButton(tr("+ Lien"));
    auto* bb = new QHBoxLayout;
    bb->addWidget(addBinding);
    bb->addStretch();
    rv->addLayout(bb);
    split->addWidget(m_details);
    split->setStretchFactor(1, 1);
    split->setSizes({ 200, 600 });

    // --- Bas : fichier + boutons
    auto* bottom = new QHBoxLayout;
    m_status = new QLabel;
    m_status->setObjectName("hintLabel");
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* reset = new QPushButton(tr("Valeurs par défaut"));
    auto* close = new QPushButton(tr("Fermer"));
    bottom->addWidget(m_status, 1);
    bottom->addWidget(reset);
    bottom->addWidget(close);
    root->addLayout(bottom);
    m_status->setText(fromFile ? tr("Fichier : %1").arg(QDir::toNativeSeparators(m_path))
                               : tr("Actions par défaut (le fichier sera créé à la première modification)"));

    // --- Connexions
    connect(m_list, &QListWidget::currentRowChanged, this, &InputSettingsDialog::showAction);
    connect(addAction, &QPushButton::clicked, this, [this] {
        QString name = tr("NouvelleAction");
        for (int i = 2; m_input.findAction(name.toStdString()); ++i) name = tr("NouvelleAction%1").arg(i);
        m_input.addAction(name.toStdString(), InputAction::Type::Button);
        save();
        rebuildActionList(int(m_input.actions().size()) - 1);
        m_name->setFocus();
        m_name->selectAll();
    });
    connect(delAction, &QPushButton::clicked, this, [this] {
        InputAction* a = currentAction();
        if (!a) return;
        const int row = m_list->currentRow();
        m_input.removeAction(a->name);
        save();
        rebuildActionList(std::min(row, int(m_input.actions().size()) - 1));
    });
    connect(m_name, &QLineEdit::editingFinished, this, [this] {
        InputAction* a = currentAction();
        const std::string name = m_name->text().trimmed().toStdString();
        if (!a || name.empty() || name == a->name) { if (a) m_name->setText(QString::fromStdString(a->name)); return; }
        if (m_input.findAction(name)) {   // deux actions ne peuvent pas avoir le même nom
            m_name->setText(QString::fromStdString(a->name));
            return;
        }
        a->name = name;
        save();
        m_list->currentItem()->setText(QString::fromStdString(name));
    });
    connect(m_type, &QComboBox::activated, this, [this](int i) {
        if (InputAction* a = currentAction()) { a->type = i == 1 ? InputAction::Type::Axis : InputAction::Type::Button; save(); }
    });
    connect(m_deadZone, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (m_updating) return;
        if (InputAction* a = currentAction()) { a->deadZone = float(v); save(); }
    });
    connect(addBinding, &QPushButton::clicked, this, [this] {
        InputAction* a = currentAction();
        if (!a) return;
        a->bindings.push_back({ Source::Key, int(sf::Keyboard::Key::Space), 1.f, 0 });
        save();
        rebuildBindings();
        // on propose tout de suite d'appuyer sur la touche
        if (auto* b = qobject_cast<KeyCaptureButton*>(m_bindings->cellWidget(m_bindings->rowCount() - 1, 1))) b->click();
    });
    connect(reset, &QPushButton::clicked, this, [this] {
        if (!m_defaults) return;
        if (QMessageBox::question(this, tr("Valeurs par défaut"),
                                  tr("Remplacer toutes les actions par celles par défaut du projet ?")) != QMessageBox::Yes)
            return;
        m_defaults(m_input);
        save();
        rebuildActionList(0);
    });
    connect(close, &QPushButton::clicked, this, &QDialog::close);

    rebuildActionList(0);
}

InputAction* InputSettingsDialog::currentAction() {
    const int row = m_list->currentRow();
    auto& actions = m_input.actions();
    return (row >= 0 && row < int(actions.size())) ? &actions[std::size_t(row)] : nullptr;
}

void InputSettingsDialog::rebuildActionList(int select) {
    m_updating = true;
    m_list->clear();
    for (const InputAction& a : m_input.actions()) m_list->addItem(QString::fromStdString(a.name));
    m_updating = false;
    m_list->setCurrentRow(select);
    showAction(m_list->currentRow());
}

void InputSettingsDialog::showAction(int) {
    InputAction* a = currentAction();
    m_details->setEnabled(a != nullptr);
    m_updating = true;
    m_name->setText(a ? QString::fromStdString(a->name) : QString());
    m_type->setCurrentIndex(a && a->type == InputAction::Type::Axis ? 1 : 0);
    m_deadZone->setValue(a ? a->deadZone : 0.2);
    m_updating = false;
    rebuildBindings();
}

void InputSettingsDialog::rebuildBindings() {
    m_bindings->setRowCount(0);
    InputAction* a = currentAction();
    if (!a) return;
    const std::string actionName = a->name;   // l'action est retrouvée par son nom (le vecteur peut bouger)
    auto binding = [this, actionName](int row) -> InputBinding* {
        InputAction* act = m_input.findAction(actionName);
        return (act && row < int(act->bindings.size())) ? &act->bindings[std::size_t(row)] : nullptr;
    };
    auto later = [this] { QTimer::singleShot(0, this, &InputSettingsDialog::rebuildBindings); };   // pas pendant le signal

    m_bindings->setRowCount(int(a->bindings.size()));
    for (int row = 0; row < int(a->bindings.size()); ++row) {
        const InputBinding& b = a->bindings[std::size_t(row)];

        // Source
        auto* source = new QComboBox;
        for (Source s : kSources) source->addItem(sourceLabel(s));
        source->setCurrentIndex(int(b.source));
        connect(source, &QComboBox::activated, this, [this, binding, row, later](int i) {
            InputBinding* bb = binding(row);
            if (!bb || int(bb->source) == i) return;
            bb->source = Source(i);
            bb->code = (bb->source == Source::Key) ? int(sf::Keyboard::Key::Space) : 0;
            save();
            later();
        });
        m_bindings->setCellWidget(row, 0, source);

        // Entrée (dépend de la source)
        QWidget* input = nullptr;
        switch (b.source) {
        case Source::Key: {
            auto* key = new KeyCaptureButton(sf::Keyboard::Key(b.code));
            connect(key, &KeyCaptureButton::keyCaptured, this, [this, binding, row](sf::Keyboard::Key k) {
                if (InputBinding* bb = binding(row)) { bb->code = int(k); save(); }
            });
            input = key;
            break;
        }
        case Source::MouseButton: {
            auto* c = new QComboBox;
            c->addItems({ tr("Gauche"), tr("Droit"), tr("Milieu"), tr("Bouton 4"), tr("Bouton 5") });
            c->setCurrentIndex(b.code);
            connect(c, &QComboBox::activated, this, [this, binding, row](int i) {
                if (InputBinding* bb = binding(row)) { bb->code = i; save(); }
            });
            input = c;
            break;
        }
        case Source::MouseWheel: {
            auto* l = new QLabel(tr("crans de molette (haut = +1)"));
            l->setObjectName("hintLabel");
            input = l;
            break;
        }
        case Source::JoystickButton: {
            auto* s = new QSpinBox;
            s->setRange(0, 31);
            s->setPrefix(tr("Bouton "));
            s->setValue(b.code);
            connect(s, &QSpinBox::valueChanged, this, [this, binding, row](int v) {
                if (InputBinding* bb = binding(row)) { bb->code = v; save(); }
            });
            input = s;
            break;
        }
        case Source::JoystickAxis: {
            auto* c = new QComboBox;
            for (int i = 0; i < 8; ++i) c->addItem(QString::fromStdString(InputManager::joystickAxisName(i)));
            c->setItemText(0, tr("X (stick gauche)"));
            c->setItemText(1, tr("Y (stick gauche)"));
            c->setCurrentIndex(b.code);
            connect(c, &QComboBox::activated, this, [this, binding, row](int i) {
                if (InputBinding* bb = binding(row)) { bb->code = i; save(); }
            });
            input = c;
            break;
        }
        }
        m_bindings->setCellWidget(row, 1, input);

        // Échelle : contribution à l'axe (-1 = sens négatif)
        auto* scale = new QDoubleSpinBox;
        scale->setRange(-100, 100);
        scale->setSingleStep(0.5);
        scale->setValue(b.scale);
        scale->setToolTip(tr("Valeur donnée à l'axe quand ce lien est actif (-1 : gauche / haut)"));
        connect(scale, &QDoubleSpinBox::valueChanged, this, [this, binding, row](double v) {
            if (InputBinding* bb = binding(row)) { bb->scale = float(v); save(); }
        });
        m_bindings->setCellWidget(row, 2, scale);

        // Numéro de manette
        auto* pad = new QSpinBox;
        pad->setRange(0, 7);
        pad->setValue(int(b.joystick));
        pad->setEnabled(b.source == Source::JoystickButton || b.source == Source::JoystickAxis);
        connect(pad, &QSpinBox::valueChanged, this, [this, binding, row](int v) {
            if (InputBinding* bb = binding(row)) { bb->joystick = unsigned(v); save(); }
        });
        m_bindings->setCellWidget(row, 3, pad);

        // Supprimer le lien
        auto* remove = new QToolButton;
        remove->setText(QString::fromUtf8("✕"));
        remove->setToolTip(tr("Supprimer ce lien"));
        connect(remove, &QToolButton::clicked, this, [this, actionName, row, later] {
            InputAction* act = m_input.findAction(actionName);
            if (!act || row >= int(act->bindings.size())) return;
            act->bindings.erase(act->bindings.begin() + row);
            save();
            later();
        });
        m_bindings->setCellWidget(row, 4, remove);
    }
}

void InputSettingsDialog::save() {
    if (m_updating) return;
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile out(m_path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)
        || out.write(QByteArray::fromStdString(m_input.toString())) < 0 || !out.commit()) {
        m_status->setText(tr("Impossible d'enregistrer %1").arg(QDir::toNativeSeparators(m_path)));
        return;
    }
    m_status->setText(tr("Enregistré dans %1").arg(QDir::toNativeSeparators(m_path)));
    emit saved();
}
