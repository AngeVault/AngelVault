#include "inspector_panel.h"
#include "component_descriptors.h"
#include "editor_context.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPointF>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {
// La molette ne modifie un champ numérique que s'il a le focus (sinon elle fait défiler le panneau)
class WheelGuard : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject* o, QEvent* e) override {
        if (e->type() == QEvent::Wheel) {
            auto* w = qobject_cast<QWidget*>(o);
            if (w && !w->hasFocus()) { e->ignore(); return true; }
        }
        return false;
    }
};

QDoubleSpinBox* makeSpin(const FieldDesc& f, QObject* guard) {
    auto* s = new QDoubleSpinBox;
    s->setRange(f.min, f.max);
    s->setDecimals(f.decimals);
    s->setSingleStep(f.step);
    s->setKeyboardTracking(false);              // valeur appliquée à Entrée / perte du focus / flèches
    s->setFocusPolicy(Qt::StrongFocus);
    s->setButtonSymbols(QAbstractSpinBox::NoButtons);
    s->installEventFilter(guard);
    s->setMinimumWidth(50);
    return s;
}

void setSwatch(QPushButton* b, const QColor& c) {
    b->setText(c.name(QColor::HexArgb).toUpper());
    const QColor text = c.alpha() < 90 ? QColor(170, 170, 170)                       // transparent : fond du panneau
                      : c.lightness() > 140 ? QColor(20, 20, 20) : QColor(240, 240, 240);
    b->setStyleSheet(QString("QPushButton { background-color: %1; color: %2; border-radius: 6px; padding: 3px 8px; }")
                         .arg(c.name(QColor::HexArgb), text.name()));
}
}

InspectorPanel::InspectorPanel(EditorContext& context, QWidget* parent) : QWidget(parent), m_ctx(context) {
    setObjectName("inspectorPanel");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_scroll = new QScrollArea;
    m_scroll->setObjectName("inspectorScroll");
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_scroll);

    // Reconstruit au prochain tour de boucle : jamais pendant le clic d'un bouton du panneau
    connect(&m_ctx, &EditorContext::selectionChanged, this, &InspectorPanel::rebuild, Qt::QueuedConnection);
    connect(&m_ctx, &EditorContext::sceneChanged, this, [this] { if (!m_ctx.hasSelection()) rebuild(); }, Qt::QueuedConnection);
    connect(&m_ctx, &EditorContext::entityModified, this, [this](Entity e, QObject* source) {
        if (source != this && e == m_ctx.selected()) refreshValues();
    });
    rebuild();
}

void InspectorPanel::rebuild() {
    m_refreshers.clear();
    auto* content = new QWidget;
    content->setObjectName("inspectorContent");
    auto* v = new QVBoxLayout(content);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(8);

    if (!m_ctx.hasSelection()) {
        auto* empty = new QLabel(tr("Sélectionnez une entité dans la Hierarchy\nou dans la vue pour voir ses composants."));
        empty->setObjectName("inspectorEmpty");
        empty->setAlignment(Qt::AlignCenter);
        empty->setWordWrap(true);
        v->addStretch();
        v->addWidget(empty);
        v->addStretch();
        m_scroll->setWidget(content);
        return;
    }

    const Entity e = m_ctx.selected();
    Registry& reg = m_ctx.scene();
    v->addWidget(buildHeader(e));

    for (const ComponentDesc& desc : componentDescriptors()) {
        if (!desc.has(reg, e)) continue;
        QFormLayout* form = nullptr;
        const QString name = desc.name;
        auto onRemove = [this, e, remove = desc.remove] {
            remove(m_ctx.scene(), e);
            m_ctx.notifyEntityModified(e, this);
            QMetaObject::invokeMethod(this, &InspectorPanel::rebuild, Qt::QueuedConnection);   // pas pendant le clic
        };
        v->addWidget(buildSection(name, bool(desc.remove), onRemove, form));
        for (const FieldDesc& field : desc.fields(reg, e))
            form->addRow(field.name, createEditor(field, e));
    }

    v->addWidget(buildAddButton(e));
    v->addStretch();
    m_scroll->setWidget(content);   // l'ancien contenu est détruit
}

QWidget* InspectorPanel::buildHeader(Entity e) {
    auto* header = new QWidget;
    header->setObjectName("inspectorHeader");
    auto* h = new QHBoxLayout(header);
    h->setContentsMargins(0, 0, 0, 0);
    auto* name = new QLineEdit(m_ctx.displayName(e));
    name->setObjectName("inspectorName");
    auto* id = new QLabel(QString("#%1").arg(e.index));
    id->setObjectName("inspectorId");
    h->addWidget(name, 1);
    h->addWidget(id);
    connect(name, &QLineEdit::editingFinished, this, [this, e, name] {
        if (!m_ctx.isValid(e)) return;
        const QString text = name->text().trimmed();
        if (!text.isEmpty() && text != m_ctx.displayName(e)) m_ctx.renameEntity(e, text);
    });
    m_refreshers.push_back([this, e, name] {
        if (!name->hasFocus()) name->setText(m_ctx.displayName(e));
    });
    return header;
}

QWidget* InspectorPanel::buildSection(const QString& title, bool removable, const std::function<void()>& onRemove,
                                      QFormLayout*& form) {
    auto* section = new QWidget;
    section->setObjectName("componentSection");
    section->setAttribute(Qt::WA_StyledBackground);
    auto* v = new QVBoxLayout(section);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    auto* header = new QWidget;
    header->setObjectName("componentHeader");
    header->setAttribute(Qt::WA_StyledBackground);
    auto* h = new QHBoxLayout(header);
    h->setContentsMargins(4, 2, 4, 2);
    auto* toggle = new QToolButton;
    toggle->setObjectName("componentToggle");
    toggle->setText(title);
    toggle->setCheckable(true);
    toggle->setChecked(!m_collapsed.contains(title));
    toggle->setArrowType(toggle->isChecked() ? Qt::DownArrow : Qt::RightArrow);
    toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    h->addWidget(toggle, 1);
    if (removable) {
        auto* remove = new QToolButton;
        remove->setObjectName("componentRemove");
        remove->setText(QString::fromUtf8("✕"));
        remove->setToolTip(tr("Retirer le composant"));
        h->addWidget(remove);
        connect(remove, &QToolButton::clicked, this, [onRemove] { onRemove(); });
    }
    v->addWidget(header);

    auto* body = new QWidget;
    body->setObjectName("componentBody");
    form = new QFormLayout(body);
    form->setContentsMargins(10, 6, 10, 8);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(5);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    body->setVisible(toggle->isChecked());
    v->addWidget(body);

    connect(toggle, &QToolButton::toggled, this, [this, toggle, body, title](bool open) {
        body->setVisible(open);
        toggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        if (open) m_collapsed.remove(title); else m_collapsed.insert(title);
    });
    return section;
}

QWidget* InspectorPanel::buildAddButton(Entity e) {
    auto* button = new QPushButton(tr("Ajouter un composant"));
    button->setObjectName("addComponentButton");
    auto* menu = new QMenu(button);
    Registry& reg = m_ctx.scene();
    for (const ComponentDesc& desc : componentDescriptors()) {
        if (!desc.add || desc.has(reg, e)) continue;
        QAction* a = menu->addAction(desc.name);
        connect(a, &QAction::triggered, this, [this, e, add = desc.add] {
            if (!m_ctx.isValid(e)) return;
            add(m_ctx.scene(), e);
            m_ctx.notifyEntityModified(e, this);
            QMetaObject::invokeMethod(this, &InspectorPanel::rebuild, Qt::QueuedConnection);
        });
    }
    if (menu->isEmpty()) {
        menu->addAction(tr("Tous les composants sont déjà présents"))->setEnabled(false);
    }
    button->setMenu(menu);
    return button;
}

void InspectorPanel::edited(Entity e) {
    m_ctx.notifyEntityModified(e, this);
}

QWidget* InspectorPanel::createEditor(const FieldDesc& f, Entity e) {
    static WheelGuard* guard = new WheelGuard(qApp);
    const bool readOnly = !f.set;
    auto get = f.get;
    auto set = f.set;

    switch (f.kind) {
    case FieldKind::Float: {
        auto* s = makeSpin(f, guard);
        s->setReadOnly(readOnly);
        s->setValue(get().toDouble());
        if (!readOnly)
            connect(s, &QDoubleSpinBox::valueChanged, this, [this, e, set](double v) { set(v); edited(e); });
        m_refreshers.push_back([s, get] { if (!s->hasFocus()) { QSignalBlocker b(s); s->setValue(get().toDouble()); } });
        return s;
    }
    case FieldKind::Int: {
        auto* s = new QSpinBox;
        s->setRange(int(std::max(f.min, -2147483647.0)), int(std::min(f.max, 2147483647.0)));
        s->setKeyboardTracking(false);
        s->setFocusPolicy(Qt::StrongFocus);
        s->setButtonSymbols(QAbstractSpinBox::NoButtons);
        s->installEventFilter(guard);
        s->setReadOnly(readOnly);
        s->setValue(get().toInt());
        if (!readOnly)
            connect(s, &QSpinBox::valueChanged, this, [this, e, set](int v) { set(v); edited(e); });
        m_refreshers.push_back([s, get] { if (!s->hasFocus()) { QSignalBlocker b(s); s->setValue(get().toInt()); } });
        return s;
    }
    case FieldKind::Bool: {
        auto* c = new QCheckBox;
        c->setChecked(get().toBool());
        c->setEnabled(!readOnly);
        if (!readOnly)
            connect(c, &QCheckBox::toggled, this, [this, e, set](bool v) { set(v); edited(e); });
        m_refreshers.push_back([c, get] { QSignalBlocker b(c); c->setChecked(get().toBool()); });
        return c;
    }
    case FieldKind::Vec2: {
        auto* w = new QWidget;
        auto* h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(4);
        auto* x = makeSpin(f, guard);
        auto* y = makeSpin(f, guard);
        for (auto [label, spin] : { std::pair{ "X", x }, std::pair{ "Y", y } }) {
            auto* l = new QLabel(label);
            l->setObjectName(QString("axisLabel%1").arg(label));
            h->addWidget(l);
            h->addWidget(spin, 1);
            spin->setReadOnly(readOnly);
        }
        const QPointF p = get().toPointF();
        x->setValue(p.x());
        y->setValue(p.y());
        if (!readOnly) {
            auto apply = [this, e, set, x, y] { set(QPointF(x->value(), y->value())); edited(e); };
            connect(x, &QDoubleSpinBox::valueChanged, this, apply);
            connect(y, &QDoubleSpinBox::valueChanged, this, apply);
        }
        m_refreshers.push_back([x, y, get] {
            const QPointF v = get().toPointF();
            if (!x->hasFocus()) { QSignalBlocker b(x); x->setValue(v.x()); }
            if (!y->hasFocus()) { QSignalBlocker b(y); y->setValue(v.y()); }
        });
        return w;
    }
    case FieldKind::String: {
        auto* l = new QLineEdit(get().toString());
        l->setReadOnly(readOnly);
        if (!readOnly)
            connect(l, &QLineEdit::editingFinished, this, [this, e, set, l] { set(l->text()); edited(e); });
        m_refreshers.push_back([l, get] { if (!l->hasFocus()) l->setText(get().toString()); });
        return l;
    }
    case FieldKind::Color: {
        auto* b = new QPushButton;
        b->setObjectName("colorSwatch");
        setSwatch(b, get().value<QColor>());
        b->setEnabled(!readOnly);
        if (!readOnly)
            connect(b, &QPushButton::clicked, this, [this, e, get, set, b, title = f.name] {
                const QColor c = QColorDialog::getColor(get().value<QColor>(), this, title, QColorDialog::ShowAlphaChannel);
                if (!c.isValid()) return;
                set(c);
                setSwatch(b, c);
                edited(e);
            });
        m_refreshers.push_back([b, get] { setSwatch(b, get().value<QColor>()); });
        return b;
    }
    case FieldKind::Label:
    default: {
        auto* l = new QLabel(get().toString());
        l->setObjectName("fieldLabel");
        m_refreshers.push_back([l, get] { l->setText(get().toString()); });
        return l;
    }
    }
}

void InspectorPanel::refreshValues() {
    if (!m_ctx.hasSelection()) return;
    for (auto& refresh : m_refreshers) refresh();
}
