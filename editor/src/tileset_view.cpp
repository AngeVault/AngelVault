#include "tileset_view.h"

#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kGap = 1;   // espace entre deux tiles affichées (px écran)
}

TilesetView::TilesetView(QWidget* parent) : QWidget(parent) {
    setObjectName("tilesetView");
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    updateSize();
}

void TilesetView::setTileset(const Tileset* tileset, const QString& imagePath) {
    const bool same = tileset && m_hasTileset && imagePath == m_imagePath && tileset->tileSize == m_ts.tileSize;
    m_hasTileset = tileset != nullptr;
    if (tileset) m_ts = *tileset;
    if (!same) {
        m_imagePath = imagePath;
        m_pixmap = imagePath.isEmpty() ? QPixmap() : QPixmap(imagePath);
        m_userZoomed = false;
        m_selection = {};
    }
    updateSize();
    update();
}

void TilesetView::reloadImage() {
    m_pixmap = m_imagePath.isEmpty() ? QPixmap() : QPixmap(m_imagePath);
    updateSize();
    update();
}

void TilesetView::setSelection(const QRect& tiles) {
    if (tiles == m_selection) return;
    m_selection = tiles;
    update();
}

void TilesetView::setDimmed(bool dimmed) {
    if (dimmed == m_dimmed) return;
    m_dimmed = dimmed;
    update();
}

void TilesetView::setLabels(const QHash<int, QString>& labels) {
    m_labels = labels;
    update();
}

int TilesetView::cellW() const { return std::max(4, int(std::lround(m_ts.tileSize.x * m_zoom))); }
int TilesetView::cellH() const { return std::max(4, int(std::lround(m_ts.tileSize.y * m_zoom))); }

void TilesetView::setZoom(double zoom) {
    zoom = std::clamp(zoom, 0.05, 8.0);
    if (std::abs(zoom - m_zoom) < 1e-6) return;
    m_zoom = zoom;
    updateSize();
    update();
    emit zoomChanged(m_zoom);
}

void TilesetView::fitWidth(int width) {
    if (!m_hasTileset || m_ts.columns <= 0 || m_ts.tileSize.x <= 0) return;
    const double avail = double(width - kGap * (m_ts.columns + 1));
    double z = avail / (double(m_ts.columns) * m_ts.tileSize.x);
    // Lisible : une tile affichée fait au moins 28 px (la grille défile alors), et pas plus de 4x
    const double minZ = 28.0 / std::max(m_ts.tileSize.x, m_ts.tileSize.y);
    z = std::clamp(z, std::min(minZ, 4.0), 4.0);
    setZoom(z);
}

QSize TilesetView::sizeHint() const {
    if (!m_hasTileset || m_ts.count <= 0) return { 200, 80 };
    return { kGap + m_ts.columns * (cellW() + kGap), kGap + m_ts.rows() * (cellH() + kGap) };
}

void TilesetView::updateSize() { setFixedSize(sizeHint()); }

QRect TilesetView::cellRect(int col, int row) const {
    return QRect(kGap + col * (cellW() + kGap), kGap + row * (cellH() + kGap), cellW(), cellH());
}

QPoint TilesetView::tileAt(const QPoint& pos) const {
    if (!m_hasTileset) return { -1, -1 };
    const int col = (pos.x() - kGap) / (cellW() + kGap);
    const int row = (pos.y() - kGap) / (cellH() + kGap);
    if (pos.x() < kGap || pos.y() < kGap || col >= m_ts.columns || row >= m_ts.rows()) return { -1, -1 };
    if (m_ts.tileIndex(col, row) < 0) return { -1, -1 };
    return { col, row };
}

void TilesetView::paintEvent(QPaintEvent* e) {
    QPainter p(this);
    p.fillRect(rect(), QColor(30, 30, 34));
    if (!m_hasTileset) return;
    if (m_pixmap.isNull()) {
        p.setPen(QColor(220, 220, 220));
        p.drawText(rect().adjusted(6, 6, -6, -6), Qt::AlignCenter | Qt::TextWordWrap,
                   tr("Image introuvable :\n%1").arg(m_imagePath));
        return;
    }
    p.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 1.0);   // pixel art net en zoom avant

    // Damier sous les tiles (transparence)
    QPixmap checker(16, 16);
    checker.fill(QColor(58, 58, 64));
    { QPainter c(&checker); c.fillRect(0, 0, 8, 8, QColor(70, 70, 78)); c.fillRect(8, 8, 8, 8, QColor(70, 70, 78)); }

    const QRect dirty = e->rect();
    for (int row = 0; row < m_ts.rows(); ++row)
        for (int col = 0; col < m_ts.columns; ++col) {
            const int index = m_ts.tileIndex(col, row);
            if (index < 0) continue;
            const QRect r = cellRect(col, row);
            if (!r.intersects(dirty)) continue;
            p.drawTiledPixmap(r, checker);
            const sf::IntRect src = m_ts.rect(index);
            p.drawPixmap(r, m_pixmap, QRect(src.position.x, src.position.y, src.size.x, src.size.y));
            if (const auto it = m_labels.constFind(index); it != m_labels.constEnd()) {
                QFont f = p.font();
                f.setPointSizeF(std::max(6.5, std::min(9.0, cellH() / 4.5)));
                f.setBold(true);
                p.setFont(f);
                const QRect badge = QRect(r.topLeft(), QSize(std::min(r.width(), QFontMetrics(f).horizontalAdvance(*it) + 6),
                                                             QFontMetrics(f).height() + 2));
                p.fillRect(badge, QColor(20, 20, 24, 200));
                p.setPen(QColor(255, 215, 90));
                p.drawText(badge, Qt::AlignCenter, *it);
            }
        }

    // Survol
    if (m_hover.x() >= 0) {
        p.setPen(QPen(QColor(255, 255, 255, 140), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(cellRect(m_hover.x(), m_hover.y()).adjusted(0, 0, -1, -1));
    }
    // Sélection (bloc)
    if (m_mode == Mode::Select && m_selection.isValid()) {
        const QRect a = cellRect(m_selection.left(), m_selection.top());
        const QRect b = cellRect(m_selection.right(), m_selection.bottom());
        const QRect sel = a.united(b).adjusted(-1, -1, 0, 0);
        const QColor c = m_dimmed ? QColor(150, 150, 150) : QColor(47, 111, 235);
        p.fillRect(sel, QColor(c.red(), c.green(), c.blue(), m_dimmed ? 25 : 55));
        p.setPen(QPen(c, 2));
        p.drawRect(sel);
    }
}

void TilesetView::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return QWidget::mousePressEvent(e);
    const QPoint t = tileAt(e->position().toPoint());
    if (t.x() < 0) return;
    if (m_mode == Mode::Click) {
        emit tileClicked(m_ts.tileIndex(t.x(), t.y()));
        return;
    }
    m_dragStart = t;
    m_selection = QRect(t, QSize(1, 1));
    update();
}

void TilesetView::mouseMoveEvent(QMouseEvent* e) {
    const QPoint t = tileAt(e->position().toPoint());
    if (t != m_hover) {
        m_hover = t;
        update();
        if (t.x() >= 0) {
            const int index = m_ts.tileIndex(t.x(), t.y());
            QString tip = tr("Tile %1  (colonne %2, ligne %3)").arg(index).arg(t.x()).arg(t.y());
            if (const auto it = m_labels.constFind(index); it != m_labels.constEnd()) tip += "\n" + *it;
            setToolTip(tip);
        }
    }
    if (m_mode == Mode::Select && m_dragStart.x() >= 0 && (e->buttons() & Qt::LeftButton) && t.x() >= 0) {
        const QRect r = QRect(m_dragStart, t).normalized();
        if (r != m_selection) { m_selection = r; update(); }
    }
}

void TilesetView::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && m_mode == Mode::Select && m_dragStart.x() >= 0) {
        m_dragStart = { -1, -1 };
        emit selectionChanged(m_selection);
    }
    QWidget::mouseReleaseEvent(e);
}

void TilesetView::wheelEvent(QWheelEvent* e) {
    if (!(e->modifiers() & Qt::ControlModifier)) { e->ignore(); return; }   // la zone défile
    const double steps = e->angleDelta().y() / 120.0;
    if (steps == 0.0) return;
    m_userZoomed = true;
    setZoom(m_zoom * std::pow(1.2, steps));
    e->accept();
}

void TilesetView::leaveEvent(QEvent* e) {
    m_hover = { -1, -1 };
    update();
    QWidget::leaveEvent(e);
}
