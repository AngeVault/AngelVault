#include "tile_palette_panel.h"

#include <QAction>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kTypeRole = Qt::UserRole + 1;
constexpr int kIcon = 32;

// Aperçu d'un terrain : la case "pleine" (colonne 15, ligne 0) de sa feuille dual-grid
QIcon terrainIcon(TileType type) {
    const QStringList files = tilesetFiles(type);
    QPixmap pix(kIcon, kIcon);
    pix.fill(Qt::transparent);
    if (!files.isEmpty()) {
        const QImage sheet(files.last());           // la dernière feuille montre les variantes (ex. fleurs)
        if (!sheet.isNull()) {
            const int cell = 16;
            const QImage tile = sheet.copy(15 * cell, 0, cell, cell).scaled(kIcon, kIcon, Qt::IgnoreAspectRatio,
                                                                           Qt::FastTransformation);
            QPainter p(&pix);
            p.drawImage(0, 0, tile);
        }
    }
    return QIcon(pix);
}
}

TilePalettePanel::TilePalettePanel(EditorContext& context, QWidget* parent) : QWidget(parent), m_ctx(context) {
    setObjectName("tilePalettePanel");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);

    // --- Outils ---
    auto* toolsRow = new QHBoxLayout;
    toolsRow->setSpacing(4);
    m_tools = new QButtonGroup(this);
    m_tools->setExclusive(true);
    struct ToolDef { EditorTool tool; QString text; QString tip; QKeySequence key; };
    const ToolDef defs[] = {
        { EditorTool::Select, tr("Sélection"), tr("Sélectionner et déplacer des entités (V)"), QKeySequence(Qt::Key_V) },
        { EditorTool::Paint,  tr("Pinceau"),   tr("Peindre le terrain choisi (B)"),             QKeySequence(Qt::Key_B) },
        { EditorTool::Erase,  tr("Gomme"),     tr("Effacer des tiles (E)"),                      QKeySequence(Qt::Key_E) },
        { EditorTool::Pick,   tr("Pipette"),   tr("Choisir le terrain d'une case (I)"),          QKeySequence(Qt::Key_I) },
    };
    for (const ToolDef& d : defs) {
        auto* b = new QToolButton;
        b->setObjectName("paletteTool");
        b->setText(d.text);
        b->setToolTip(d.tip);
        b->setCheckable(true);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_tools->addButton(b, int(d.tool));
        toolsRow->addWidget(b);

        // Raccourci (un champ texte qui a le focus garde ses lettres)
        auto* shortcut = new QAction(d.text, this);
        shortcut->setShortcut(d.key);
        shortcut->setShortcutContext(Qt::WindowShortcut);
        const EditorTool tool = d.tool;
        connect(shortcut, &QAction::triggered, this, [this, tool] { m_ctx.setTool(tool); });
        m_toolActions.push_back(shortcut);
    }
    layout->addLayout(toolsRow);
    connect(m_tools, &QButtonGroup::idClicked, this, [this](int id) { m_ctx.setTool(EditorTool(id)); });

    // --- Taille du pinceau ---
    auto* brushRow = new QHBoxLayout;
    brushRow->addWidget(new QLabel(tr("Taille du pinceau")));
    m_brush = new QSpinBox;
    m_brush->setRange(1, 9);
    m_brush->setButtonSymbols(QAbstractSpinBox::NoButtons);   // molette ou clavier
    m_brush->setValue(m_ctx.brushSize());
    m_brush->setSuffix(tr(" case(s)"));
    brushRow->addWidget(m_brush, 1);
    layout->addLayout(brushRow);
    connect(m_brush, &QSpinBox::valueChanged, &m_ctx, &EditorContext::setBrushSize);

    // --- Terrains ---
    auto* title = new QLabel(tr("Terrains"));
    title->setObjectName("paletteSectionTitle");
    layout->addWidget(title);
    m_terrains = new QListWidget;
    m_terrains->setObjectName("paletteTerrains");
    // Liste : aperçu à gauche, nom à droite (le texte ne peut pas déborder sur les autres cases)
    m_terrains->setViewMode(QListView::ListMode);
    m_terrains->setIconSize(QSize(kIcon, kIcon));
    m_terrains->setUniformItemSizes(true);
    m_terrains->setSpacing(1);
    layout->addWidget(m_terrains, 1);
    buildTerrains();
    connect(m_terrains, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        if (!item) return;
        m_ctx.setPaintType(TileType(item->data(kTypeRole).toInt()));
        if (m_ctx.tool() == EditorTool::Select || m_ctx.tool() == EditorTool::Pick) m_ctx.setTool(EditorTool::Paint);
    });

    auto* help = new QLabel(tr("Clic gauche : peindre  ·  clic droit / molette : déplacer la vue"));
    help->setObjectName("paletteHelp");
    help->setWordWrap(true);
    layout->addWidget(help);

    connect(&m_ctx, &EditorContext::toolChanged, this, &TilePalettePanel::syncTool);
    connect(&m_ctx, &EditorContext::paintTypeChanged, this, &TilePalettePanel::syncTerrain);
    connect(&m_ctx, &EditorContext::brushSizeChanged, this, [this](int s) { QSignalBlocker b(m_brush); m_brush->setValue(s); });
    syncTool(m_ctx.tool());
    syncTerrain(m_ctx.paintType());
}

void TilePalettePanel::buildTerrains() {
    for (TileType type : paintableTileTypes()) {
        auto* item = new QListWidgetItem(terrainIcon(type), tileTypeName(type));
        item->setData(kTypeRole, int(type));
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item->setSizeHint(QSize(0, kIcon + 10));
        m_terrains->addItem(item);
    }
}

void TilePalettePanel::syncTool(EditorTool tool) {
    if (QAbstractButton* b = m_tools->button(int(tool))) b->setChecked(true);
}

void TilePalettePanel::syncTerrain(TileType type) {
    QSignalBlocker block(m_terrains);
    for (int i = 0; i < m_terrains->count(); ++i)
        if (m_terrains->item(i)->data(kTypeRole).toInt() == int(type)) m_terrains->setCurrentRow(i);
}
