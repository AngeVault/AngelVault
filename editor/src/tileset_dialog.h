#pragma once
#include "tilemap/tileset.h"

#include <QDialog>
#include <QSize>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class TilesetView;

// Fenêtre « Tileset » : crée ou modifie un tileset (panneau Tile Palette > + / Modifier).
//  - Image (feuille) + taille d'une tile, marge, espacement : la grille est calculée seule.
//  - Type : simple (on choisit la tile à la main) ou autotile (4 bords / 16 coins).
//  - Autotile 4 bords : on clique sur les tiles de la feuille pour leur donner un rôle
//    (case pleine, 1 bord, 2 bords, 4 bords, dessous). « Disposition standard » remplit tout d'un coup.
// Le tileset est enregistré dans un fichier .tileset (chemins relatifs au projet).
class TilesetDialog : public QDialog {
    Q_OBJECT
public:
    TilesetDialog(const QString& projectDir, const Tileset& tileset, bool isNew, bool isometric, QWidget* parent = nullptr);
    Tileset result() const;                 // `file` = chemin du .tileset relatif au projet
    QString absoluteFile() const;

private:
    enum Role { Full, Edge0, Edge1, Edge2, Edge3, Corner0, Corner1, Corner2, Corner3, Ring, Under, RoleCount };

    void updateGrid();                      // image / taille / marge changées
    void updateMode();
    void updateLabels();
    void updateFileLabel();
    void onTileClicked(int index);
    QString pickImage();
    QString relative(const QString& absolute) const;
    QString absolute(const QString& path) const;
    void accept() override;

    QString m_projectDir;
    Tileset m_ts;
    bool m_isNew = false;
    bool m_iso = false;
    bool m_fileChosen = false;              // nouveau tileset : chemin choisi à la main
    QString m_file;                         // .tileset, relatif au projet
    QSize m_imageSize;

    QLineEdit* m_name = nullptr;
    QLineEdit* m_image = nullptr;
    QSpinBox* m_tileW = nullptr;
    QSpinBox* m_tileH = nullptr;
    QSpinBox* m_margin = nullptr;
    QSpinBox* m_spacing = nullptr;
    QLabel* m_gridInfo = nullptr;
    QComboBox* m_mode = nullptr;
    QGroupBox* m_edgesBox = nullptr;
    QComboBox* m_role = nullptr;
    QGroupBox* m_cornersBox = nullptr;
    QLabel* m_cornersInfo = nullptr;
    QDoubleSpinBox* m_frameTime = nullptr;
    QCheckBox* m_autoAnchor = nullptr;
    QDoubleSpinBox* m_anchorX = nullptr;
    QDoubleSpinBox* m_anchorY = nullptr;
    QDoubleSpinBox* m_scale = nullptr;
    QCheckBox* m_solid = nullptr;
    QLabel* m_fileLabel = nullptr;
    QLabel* m_viewHint = nullptr;
    TilesetView* m_view = nullptr;
};
