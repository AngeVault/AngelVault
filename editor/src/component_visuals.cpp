#include "component_visuals.h"
#include "component_descriptors.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QtMath>

namespace {
QPolygonF poly(std::initializer_list<QPointF> points) { return QPolygonF(QList<QPointF>(points)); }

void line(QPainter& p, qreal x1, qreal y1, qreal x2, qreal y2) { p.drawLine(QPointF(x1, y1), QPointF(x2, y2)); }

void dot(QPainter& p, qreal x, qreal y, qreal r) {
    p.save();
    p.setBrush(p.pen().color());
    p.drawEllipse(QPointF(x, y), r, r);
    p.restore();
}

QPolygonF star(QPointF c, qreal outer, qreal inner) {
    QPolygonF s;
    for (int i = 0; i < 10; ++i) {
        const qreal a = qDegreesToRadians(-90.0 + i * 36.0);
        const qreal r = i % 2 ? inner : outer;
        s << QPointF(c.x() + r * std::cos(a), c.y() + r * std::sin(a));
    }
    return s;
}

QPainterPath folderPath() {
    QPainterPath path;
    path.addPolygon(poly({ { 2.5, 5.5 }, { 9, 5.5 }, { 11, 8 }, { 21.5, 8 }, { 21.5, 19.5 }, { 2.5, 19.5 } }));
    path.closeSubpath();
    return path;
}

// tout est dessiné dans une grille 24×24
void drawShape(QPainter& p, const QString& n) {
    if (n == "move") {
        line(p, 12, 3, 12, 21); line(p, 3, 12, 21, 12);
        p.drawPolyline(poly({ { 9, 6 }, { 12, 3 }, { 15, 6 } }));
        p.drawPolyline(poly({ { 9, 18 }, { 12, 21 }, { 15, 18 } }));
        p.drawPolyline(poly({ { 6, 9 }, { 3, 12 }, { 6, 15 } }));
        p.drawPolyline(poly({ { 18, 9 }, { 21, 12 }, { 18, 15 } }));
    } else if (n == "image") {
        p.drawRoundedRect(QRectF(3, 4, 18, 16), 2, 2);
        p.drawEllipse(QPointF(8.5, 9.5), 1.8, 1.8);
        p.drawPolyline(poly({ { 21, 16 }, { 15, 10 }, { 5, 20 } }));
    } else if (n == "atom") {
        for (qreal a : { 0.0, 60.0, -60.0 }) {
            p.save();
            p.translate(12, 12);
            p.rotate(a);
            p.drawEllipse(QPointF(0, 0), 10, 4);
            p.restore();
        }
        dot(p, 12, 12, 1.4);
    } else if (n == "camera") {
        p.drawRoundedRect(QRectF(2, 7, 20, 13), 2, 2);
        p.drawPolyline(poly({ { 8, 7 }, { 9.5, 4 }, { 14.5, 4 }, { 16, 7 } }));
        p.drawEllipse(QPointF(12, 13.5), 3.5, 3.5);
    } else if (n == "grid") {
        p.drawRoundedRect(QRectF(3, 3, 18, 18), 2, 2);
        line(p, 9, 3, 9, 21); line(p, 15, 3, 15, 21);
        line(p, 3, 9, 21, 9); line(p, 3, 15, 21, 15);
    } else if (n == "text") {
        line(p, 5, 5, 19, 5); line(p, 12, 5, 12, 19); line(p, 9, 19, 15, 19);
    } else if (n == "sword") {
        line(p, 6, 18, 19, 5);
        p.drawPolyline(poly({ { 15, 5 }, { 19, 5 }, { 19, 9 } }));
        line(p, 6, 13, 11, 18); line(p, 3, 21, 6, 18);
    } else if (n == "code") {
        p.drawPolyline(poly({ { 8, 7 }, { 3, 12 }, { 8, 17 } }));
        p.drawPolyline(poly({ { 16, 7 }, { 21, 12 }, { 16, 17 } }));
    } else if (n == "cube") {
        p.drawPolygon(poly({ { 12, 2.5 }, { 20.5, 7 }, { 20.5, 17 }, { 12, 21.5 }, { 3.5, 17 }, { 3.5, 7 } }));
        p.drawPolyline(poly({ { 3.5, 7 }, { 12, 11.5 }, { 20.5, 7 } }));
        line(p, 12, 11.5, 12, 21.5);
    } else if (n == "lock") {
        p.drawRoundedRect(QRectF(5, 11, 14, 10), 2, 2);
        QPainterPath arc;
        arc.moveTo(8, 11);
        arc.lineTo(8, 7.5);
        arc.arcTo(QRectF(8, 3.5, 8, 8), 180, -180);
        arc.lineTo(16, 11);
        p.drawPath(arc);
    } else if (n == "shape") {
        p.drawEllipse(QPointF(8.5, 8.5), 5.5, 5.5);
        p.drawRoundedRect(QRectF(11, 11, 10, 10), 1.5, 1.5);
    } else if (n == "film") {
        p.drawRoundedRect(QRectF(3, 3, 18, 18), 2, 2);
        line(p, 7.5, 3, 7.5, 21); line(p, 16.5, 3, 16.5, 21); line(p, 7.5, 12, 16.5, 12);
        line(p, 3, 8, 7.5, 8); line(p, 3, 16, 7.5, 16); line(p, 16.5, 8, 21, 8); line(p, 16.5, 16, 21, 16);
    } else if (n == "sun") {
        p.drawEllipse(QPointF(12, 12), 4, 4);
        for (int i = 0; i < 8; ++i) {
            const qreal a = qDegreesToRadians(i * 45.0);
            line(p, 12 + 7 * std::cos(a), 12 + 7 * std::sin(a), 12 + 9.5 * std::cos(a), 12 + 9.5 * std::sin(a));
        }
    } else if (n == "weight") {
        p.drawPolygon(poly({ { 6, 9 }, { 18, 9 }, { 21, 21 }, { 3, 21 } }));
        p.drawEllipse(QPointF(12, 5.5), 2.5, 2.5);
    } else if (n == "hexagon") {
        p.drawPolygon(poly({ { 12, 2.5 }, { 20.5, 7.25 }, { 20.5, 16.75 }, { 12, 21.5 }, { 3.5, 16.75 }, { 3.5, 7.25 } }));
    } else if (n == "target") {
        p.drawEllipse(QPointF(12, 12), 9, 9);
        p.drawEllipse(QPointF(12, 12), 5, 5);
        dot(p, 12, 12, 1.5);
    } else if (n == "frame") {
        p.drawPolyline(poly({ { 3, 8 }, { 3, 3 }, { 8, 3 } }));
        p.drawPolyline(poly({ { 16, 3 }, { 21, 3 }, { 21, 8 } }));
        p.drawPolyline(poly({ { 21, 16 }, { 21, 21 }, { 16, 21 } }));
        p.drawPolyline(poly({ { 8, 21 }, { 3, 21 }, { 3, 16 } }));
    } else if (n == "gamepad") {
        p.drawRoundedRect(QRectF(2, 7, 20, 11), 5, 5);
        line(p, 6, 12.5, 10, 12.5); line(p, 8, 10.5, 8, 14.5);
        dot(p, 15, 11.5, 1); dot(p, 17.5, 14, 1);
    } else if (n == "shake") {
        p.drawRoundedRect(QRectF(8, 4, 8, 16), 1.5, 1.5);
        line(p, 4, 8, 4, 16); line(p, 20, 8, 20, 16);
    } else if (n == "pin") {
        QPainterPath path;
        path.moveTo(12, 22);
        path.cubicTo(4, 14, 4, 3, 12, 3);
        path.cubicTo(20, 3, 20, 14, 12, 22);
        p.drawPath(path);
        p.drawEllipse(QPointF(12, 10), 2.8, 2.8);
    } else if (n == "door") {
        p.drawRoundedRect(QRectF(5, 3, 14, 18), 1, 1);
        line(p, 2, 21, 22, 21);
        dot(p, 15, 12.5, 1.2);
    } else if (n == "button") {
        p.drawRoundedRect(QRectF(3, 7, 18, 10), 5, 5);
        line(p, 8.5, 12, 15.5, 12);
    } else if (n == "bubble") {
        p.drawRoundedRect(QRectF(3, 4, 18, 13), 3, 3);
        p.drawPolyline(poly({ { 8, 17 }, { 7, 21 }, { 12, 17 } }));
    } else if (n == "pointer") {
        p.drawPolygon(poly({ { 5, 3 }, { 19, 12 }, { 12.5, 13.5 }, { 9, 20 } }));
    } else if (n == "spark") {
        p.drawPolygon(poly({ { 12, 3 }, { 14, 10 }, { 21, 12 }, { 14, 14 }, { 12, 21 }, { 10, 14 }, { 3, 12 }, { 10, 10 } }));
    } else if (n == "gem") {
        p.drawPolygon(poly({ { 6, 4 }, { 18, 4 }, { 22, 9 }, { 12, 21 }, { 2, 9 } }));
        line(p, 2, 9, 22, 9);
    } else if (n == "chest") {
        p.drawRoundedRect(QRectF(3, 4, 18, 7), 3, 3);
        p.drawRoundedRect(QRectF(3, 11, 18, 10), 1.5, 1.5);
        p.drawRect(QRectF(10.5, 9.5, 3, 4));
    } else if (n == "flag") {
        line(p, 5, 22, 5, 3);
        p.drawPolygon(poly({ { 5, 4 }, { 19, 4 }, { 15.5, 8.5 }, { 19, 13 }, { 5, 13 } }));
    } else if (n == "check") {
        p.drawPolyline(poly({ { 4, 12.5 }, { 9.5, 18 }, { 20, 6 } }));
    } else if (n == "bag") {
        p.drawRoundedRect(QRectF(4, 8, 16, 13), 2, 2);
        p.drawArc(QRectF(8, 3, 8, 10), 0, 180 * 16);
    } else if (n == "folder") {
        p.drawPath(folderPath());
    } else if (n == "folder_plus") {
        p.drawPath(folderPath());
        line(p, 12, 10.5, 12, 17); line(p, 8.75, 13.75, 15.25, 13.75);
    } else if (n == "star") {
        p.drawPolygon(star({ 12, 12.5 }, 9.5, 4.2));
    } else if (n == "star_filled") {
        p.setBrush(p.pen().color());
        p.drawPolygon(star({ 12, 12.5 }, 9.5, 4.2));
    } else if (n == "search") {
        p.drawEllipse(QPointF(10.5, 10.5), 6.5, 6.5);
        line(p, 15.5, 15.5, 21, 21);
    } else if (n == "close") {
        line(p, 6, 6, 18, 18); line(p, 18, 6, 6, 18);
    } else if (n == "plus") {
        line(p, 12, 5, 12, 19); line(p, 5, 12, 19, 12);
    }
}
}

void drawGlyph(QPainter& p, const QString& name, const QRectF& rect, const QColor& color) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.translate(rect.topLeft());
    p.scale(rect.width() / 24.0, rect.height() / 24.0);
    QPen pen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    drawShape(p, name);
    p.restore();
}

QIcon glyphIcon(const QString& name, const QColor& color, int size) {
    QPixmap pm(size * 2, size * 2);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    drawGlyph(p, name, QRectF(0, 0, size, size), color);
    return QIcon(pm);
}

QString componentGlyph(const QString& key) {
    if (key.startsWith("behavior:")) return "code";
    const QString name = key.section(':', 1);
    for (const ComponentDesc& d : componentDescriptors())
        if (d.name == name) return d.icon;
    return "cube";
}

ComponentDropZone::ComponentDropZone(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(56);
    setVisible(false);
}

void ComponentDropZone::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor fill = m_accent;
    fill.setAlpha(28);
    QPen pen(m_accent, 1.2, Qt::DashLine);
    p.setPen(pen);
    p.setBrush(fill);
    p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 7, 7);
    const qreal s = 22;
    drawGlyph(p, m_glyph.isEmpty() ? QStringLiteral("plus") : m_glyph, QRectF(width() / 2.0 - s / 2, height() / 2.0 - s / 2, s, s), m_accent);
}
