#pragma once
#include <QColor>
#include <QIcon>
#include <QString>
#include <QWidget>

class QPainter;

// glisser une carte du panneau composants : "component:<nom>" ou "behavior:<nom>"
inline constexpr const char* kComponentMimeType = "application/x-angevault-component";

// glyphes au trait dessinés en code : suivent la couleur du thème, nets à toutes les tailles
void drawGlyph(QPainter& p, const QString& name, const QRectF& rect, const QColor& color);
QIcon glyphIcon(const QString& name, const QColor& color, int size = 16);

// glyphe d'une clé de la bibliothèque ("component:RigidBody2D", "behavior:Spin")
QString componentGlyph(const QString& key);

// zone en pointillés de l'Inspector pendant qu'on y glisse une carte
class ComponentDropZone : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor)
public:
    explicit ComponentDropZone(QWidget* parent = nullptr);
    QColor accentColor() const { return m_accent; }
    void setAccentColor(const QColor& c) { m_accent = c; update(); }
    void setGlyph(const QString& glyph) { m_glyph = glyph; update(); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QColor m_accent{ 0x3b, 0x82, 0xf6 };
    QString m_glyph;
};
