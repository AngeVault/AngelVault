#pragma once
#include "editor_context.h"

#include <QList>
#include <QWidget>

class QAction;
class QButtonGroup;
class QComboBox;
class QLabel;
class QScrollArea;
class QSlider;
class QSpinBox;
class QToolButton;
class TilesetView;

// Outils de la vue scène + tilesets de la tilemap active (Hierarchy : entité avec un composant Tilemap).
//  V : sélection   B : pinceau   E : gomme   G : remplir   I : pipette
//  Taille du pinceau : boutons rapides, curseur, Ctrl + molette dans la vue, [ ] ou - +
//
// Les tilesets sont des feuilles PNG découpées en grille (fichiers .tileset) :
//  - tileset simple   : cliquez sur une tile (ou glissez pour un bloc) puis peignez ;
//  - tileset autotile : « Autotile » peint le terrain et choisit les transitions seul
//                       (on peut aussi poser une tile précise de la feuille à la main).
// L'ordre des tilesets = l'empilement (un autotile plus bas déborde sur ceux du dessus).
class TilePalettePanel : public QWidget {
    Q_OBJECT
public:
    explicit TilePalettePanel(EditorContext& context, QWidget* parent = nullptr);

    // Raccourcis (outils, taille du pinceau) : à ajouter à la fenêtre principale (window.addActions)
    // pour qu'ils marchent même quand ce panneau est caché ou dans un onglet de fond.
    QList<QAction*> toolActions() const { return m_toolActions; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildTilesets();          // la liste des tilesets de la carte active
    void showTileset(int slot);    // la grille du tileset choisi
    void syncTool(EditorTool tool);
    void syncBrush(const TileBrush& brush);
    void syncBrushSize(int size);
    void currentSlotChanged(int slot);
    void gridSelectionChanged(const QRect& tiles);

    void newTilesetFromSheet();
    void newTilesetFromImages();
    void addExistingTileset();
    void editTileset();
    void removeTileset();
    void moveTileset(int delta);
    // Ouvre la fenêtre Tileset ; OK = enregistre le .tileset (et l'ajoute à la carte si `isNew`)
    bool runTilesetDialog(const Tileset& start, bool isNew, int slot);

    int currentSlot() const;
    QString projectDir() const;

    EditorContext& m_ctx;
    QButtonGroup* m_tools = nullptr;
    QList<QAction*> m_toolActions;

    // Pinceau
    QWidget* m_brushBox = nullptr;
    QSlider* m_brushSlider = nullptr;
    QSpinBox* m_brushSpin = nullptr;
    QButtonGroup* m_quickSizes = nullptr;
    QToolButton* m_square = nullptr;
    QToolButton* m_circle = nullptr;

    // Tilesets
    QLabel* m_mapInfo = nullptr;
    QWidget* m_tilesetBar = nullptr;
    QComboBox* m_tilesets = nullptr;
    QToolButton* m_editTs = nullptr;
    QToolButton* m_removeTs = nullptr;
    QToolButton* m_upTs = nullptr;
    QToolButton* m_downTs = nullptr;
    QToolButton* m_autotile = nullptr;
    QLabel* m_selectionInfo = nullptr;
    QScrollArea* m_scroll = nullptr;
    TilesetView* m_view = nullptr;
    int m_shownSlot = -2;
    uint32_t m_shownVersion = 0;
};
