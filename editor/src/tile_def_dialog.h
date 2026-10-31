#pragma once
#include "tilemap/tilemap.h"

#include <QDialog>
#include <array>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QSpinBox;

// Fenêtre « Tile » : crée ou modifie une tile d'une tilemap (panneau Tile Palette > + Tile / Modifier).
//  - Simple : une image (plusieurs = variantes au hasard)
//  - Autotile 16 coins : feuilles dual-grid (16 colonnes = combinaisons des 4 coins)
//  - Autotile 4 bords : pièces posées sur les cases voisines (1 bord / 2 bords / 4 bords, 4 rotations)
// Les chemins des images sont gardés relatifs au projet.
class TileDefDialog : public QDialog {
    Q_OBJECT
public:
    TileDefDialog(const TileMapData& map, const TileDef& tile, int editedIndex, QWidget* parent = nullptr);
    TileDef result() const;

private:
    void updateMode();
    QString pickImage(const QString& title);                  // chemin relatif au projet, "" = annulé
    QString relative(const QString& absolute) const;
    QString absolute(const QString& path) const;
    void fillRotations(std::array<QLineEdit*, 4>& fields, int index, const QString& path);   // _N/_E/_S/_W
    void accept() override;

    const TileMapData& m_map;
    TileDef m_tile;
    QLineEdit* m_name = nullptr;
    QComboBox* m_mode = nullptr;
    QLabel* m_imagesLabel = nullptr;
    QListWidget* m_images = nullptr;
    QGroupBox* m_cornersBox = nullptr;
    QSpinBox* m_cellPixels = nullptr;
    QDoubleSpinBox* m_frameTime = nullptr;
    QGroupBox* m_edgesBox = nullptr;
    std::array<QLineEdit*, 4> m_edge{};
    std::array<QLineEdit*, 4> m_corner{};
    QListWidget* m_ring = nullptr;
    QComboBox* m_base = nullptr;
    QCheckBox* m_autoAnchor = nullptr;
    QDoubleSpinBox* m_anchorX = nullptr;
    QDoubleSpinBox* m_anchorY = nullptr;
    QDoubleSpinBox* m_scale = nullptr;
    QCheckBox* m_solid = nullptr;
};
