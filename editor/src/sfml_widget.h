#pragma once
#include <QColor>
#include <QTimer>
#include <QWidget>
#include <SFML/Graphics/RenderWindow.hpp>
#include <functional>
#include <optional>

class SfmlWidget : public QWidget {
    Q_OBJECT
    // Couleur de fond de la vue, réglable depuis le thème QSS :
    //     SfmlWidget { qproperty-clearColor: #fbf3f6; }
    Q_PROPERTY(QColor clearColor READ clearColor WRITE setClearColor)
public:
    explicit SfmlWidget(QWidget* parent = nullptr);

    // Appelée une fois, dès que la fenêtre SFML existe (au premier affichage) : créez ici votre Game.
    std::function<void(sf::RenderWindow&)> onInit;
    // Appelée ~60 fois par seconde : dessinez dans la fenêtre. clear() et display() sont faits pour vous.
    std::function<void(sf::RenderWindow&)> onFrame;
    sf::RenderWindow* window() { return m_window ? &*m_window : nullptr; }

    QColor clearColor() const { return m_clearColor; }
    void setClearColor(const QColor& color) { if (color.isValid()) m_clearColor = color; }

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
    QColor m_clearColor{ 27, 28, 31 };
};
