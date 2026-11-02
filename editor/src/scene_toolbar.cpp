#include "scene_toolbar.h"

#include <QAction>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

namespace {
QToolButton* toolButton(const QString& text, const QString& tip) {
    auto* b = new QToolButton;
    b->setObjectName("paletteTool");   // même style que les outils de la Tile Palette
    b->setText(text);
    b->setToolTip(tip);
    b->setCheckable(true);
    return b;
}
} // namespace

SceneToolbar::SceneToolbar(EditorContext& context, QWidget* parent) : QWidget(parent), m_ctx(context) {
    setObjectName("sceneToolbar");
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(6, 4, 6, 4);
    row->setSpacing(3);

    // ---------------- Outils entités ----------------
    m_tools = new QButtonGroup(this);
    m_tools->setExclusive(false);   // Pinceau, Gomme... (Tile Palette) ne sont pas ici : aucun bouton coché alors
    struct ToolDef { EditorTool tool; QString text; QString tip; QKeySequence key; };
    const ToolDef defs[] = {
        { EditorTool::Select, tr("Sélection"), tr("Sélectionner et déplacer des entités (V)"), QKeySequence(Qt::Key_V) },
        { EditorTool::Resize, tr("Taille"),    tr("Agrandir la forme et / ou la collision de l'entité sélectionnée (R)\n"
                                                  "Maj : garder les proportions   Ctrl : arrondir à la demi-case"), QKeySequence(Qt::Key_R) },
    };
    for (const ToolDef& d : defs) {
        QToolButton* b = toolButton(d.text, d.tip);
        m_tools->addButton(b, int(d.tool));
        row->addWidget(b);

        if (d.tool == EditorTool::Select) continue;   // V : déjà fourni par la Tile Palette
        auto* shortcut = new QAction(d.text, this);
        shortcut->setShortcut(d.key);
        shortcut->setShortcutContext(Qt::WindowShortcut);   // un champ texte qui a le focus garde ses lettres
        const EditorTool tool = d.tool;
        connect(shortcut, &QAction::triggered, this, [this, tool] { m_ctx.setTool(tool); });
        m_toolActions.push_back(shortcut);
    }
    connect(m_tools, &QButtonGroup::idClicked, this, [this](int id) {
        m_ctx.setTool(EditorTool(id));
        syncTool(m_ctx.tool());   // groupe non exclusif : on remet les coches nous-mêmes
    });

    // ---------------- Outil Taille : ce que l'on agrandit ----------------
    m_resizeBox = new QWidget;
    auto* rr = new QHBoxLayout(m_resizeBox);
    rr->setContentsMargins(10, 0, 0, 0);
    rr->setSpacing(3);
    auto* title = new QLabel(tr("Agrandir"));
    title->setObjectName("paletteSectionTitle");
    rr->addWidget(title);
    auto* targets = new QButtonGroup(this);
    struct TargetDef { ResizeTarget target; QString text; QString tip; };
    const TargetDef targetDefs[] = {
        { ResizeTarget::Both,     tr("Forme + collision"), tr("La forme (Geometry) et la collision (Collider2D) grandissent ensemble") },
        { ResizeTarget::Geometry, tr("Forme"),             tr("Seulement la forme affichée (Geometry ; un sprite change d'échelle)") },
        { ResizeTarget::Collider, tr("Collision"),         tr("Seulement la collision (Collider2D) ; le bord opposé ne bouge pas") },
    };
    for (const TargetDef& t : targetDefs) {
        QToolButton* b = toolButton(t.text, t.tip);
        b->setChecked(m_ctx.resizeTarget() == t.target);
        targets->addButton(b, int(t.target));
        rr->addWidget(b);
    }
    row->addWidget(m_resizeBox);
    row->addStretch();
    connect(targets, &QButtonGroup::idClicked, this, [this](int id) { m_ctx.setResizeTarget(ResizeTarget(id)); });
    connect(&m_ctx, &EditorContext::resizeTargetChanged, this, [targets](ResizeTarget t) {
        if (QAbstractButton* b = targets->button(int(t))) b->setChecked(true);
    });

    connect(&m_ctx, &EditorContext::toolChanged, this, &SceneToolbar::syncTool);
    syncTool(m_ctx.tool());
}

void SceneToolbar::syncTool(EditorTool tool) {
    for (QAbstractButton* b : m_tools->buttons()) b->setChecked(m_tools->id(b) == int(tool));
    m_resizeBox->setVisible(tool == EditorTool::Resize);
}
