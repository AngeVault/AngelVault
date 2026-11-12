#pragma once
#include <QColor>
#include <QTimer>
#include <QWidget>
#include <QString>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <functional>
#include <optional>

class SfmlWidget : public QWidget {
    Q_OBJECT
    // Couleur de fond de la vue, réglable depuis le thème QSS :
    //     SfmlWidget { qproperty-clearColor: #fbf3f6; }
    Q_PROPERTY(QColor clearColor READ clearColor WRITE setClearColor)
    // Image de fond (derrière la scène, remplit la vue sans déformer), depuis le thème QSS :
    //     TilemapView { qproperty-backgroundImage: ":/resources/backgrounds/sakura_cat.jpg"; }
    // "" ou "none" = pas d'image. backgroundOpacity : 0 (invisible) à 1 (image pleine).
    Q_PROPERTY(QString backgroundImage READ backgroundImage WRITE setBackgroundImage)
    Q_PROPERTY(qreal backgroundOpacity READ backgroundOpacity WRITE setBackgroundOpacity)
public:
    explicit SfmlWidget(QWidget* parent = nullptr);

    // Appelée une fois, dès que la fenêtre SFML existe (au premier affichage) : créez ici votre Game.
    std::function<void(sf::RenderWindow&)> onInit;
    // Appelée ~60 fois par seconde : dessinez dans la fenêtre. clear() et display() sont faits pour vous.
    std::function<void(sf::RenderWindow&)> onFrame;
    sf::RenderWindow* window() { return m_window ? &*m_window : nullptr; }

    QColor clearColor() const { return m_clearColor; }
    void setClearColor(const QColor& color) { if (color.isValid()) m_clearColor = color; }
    QString backgroundImage() const { return m_backgroundPath; }
    void setBackgroundImage(const QString& path);
    qreal backgroundOpacity() const { return m_backgroundOpacity; }
    void setBackgroundOpacity(qreal opacity) { m_backgroundOpacity = qBound(0.0, opacity, 1.0); }

protected:
    QPaintEngine* paintEngine() const override { return nullptr; }   // SFML dessine, pas Qt
    void paintEvent(QPaintEvent*) override {}
    void showEvent(QShowEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    void renderFrame();
    void syncSize();
    std::optional<sf::RenderWindow> m_window;
    QTimer m_timer;
    void drawBackground();
    QColor m_clearColor{ 27, 28, 31 };
    QString m_backgroundPath;
    qreal m_backgroundOpacity = 1.0;
    bool m_backgroundDirty = false;               // à (re)charger au prochain rendu (contexte OpenGL actif)
    std::optional<sf::Texture> m_background;
};
