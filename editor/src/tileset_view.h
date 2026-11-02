#pragma once
#include "tilemap/tileset.h"

#include <QHash>
#include <QPixmap>
#include <QRect>
#include <QWidget>

// Grille d'un tileset : la feuille découpée en tiles, pour en choisir une (ou un bloc).
//  - clic : une tile    glisser : un bloc de tiles (tampon)    Ctrl + molette : zoom
//  - mode « clic » (fenêtre Tileset) : chaque clic émet tileClicked, pour affecter les rôles autotile
// À placer dans un QScrollArea (setWidgetResizable(false)).
class TilesetView : public QWidget {
    Q_OBJECT
public:
    enum class Mode { Select, Click };
    explicit TilesetView(QWidget* parent = nullptr);

    // `imagePath` : chemin complet de la feuille. nullptr = vide.
    void setTileset(const Tileset* tileset, const QString& imagePath);
    void reloadImage();                         // la feuille a été réécrite sur le disque
    void setMode(Mode mode) { m_mode = mode; }

    QRect selection() const { return m_selection; }   // en tiles (colonne, ligne, largeur, hauteur)
    void setSelection(const QRect& tiles);
    void setDimmed(bool dimmed);                 // autotile actif : la sélection n'est pas utilisée
    void setLabels(const QHash<int, QString>& labels);   // petit texte sur des tiles (rôles autotile)

    double zoom() const { return m_zoom; }
    void setZoom(double zoom);
    void fitWidth(int width);                    // zoom pour que la grille tienne dans `width` px
    bool userZoomed() const { return m_userZoomed; }

    QSize sizeHint() const override;

signals:
    void selectionChanged(const QRect& tiles);
    void tileClicked(int index);
    void zoomChanged(double zoom);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    QPoint tileAt(const QPoint& pos) const;      // colonne, ligne (-1 si hors grille)
    QRect cellRect(int col, int row) const;      // à l'écran
    int cellW() const;
    int cellH() const;
    void updateSize();

    Tileset m_ts;
    bool m_hasTileset = false;
    QString m_imagePath;
    QPixmap m_pixmap;
    Mode m_mode = Mode::Select;
    double m_zoom = 1.0;
    bool m_userZoomed = false;
    QRect m_selection;
    bool m_dimmed = false;
    QHash<int, QString> m_labels;
    QPoint m_dragStart{ -1, -1 };
    QPoint m_hover{ -1, -1 };
};
