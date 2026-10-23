#pragma once
#include <QTimer>
#include <QWidget>
#include <SFML/Graphics/RenderWindow.hpp>
#include <functional>
#include <optional>

// Le seul widget Qt de la base : une fenêtre SFML incrustée dans Qt.
// A VALIDER EN PREMIER sous Windows (voir docs/ROADMAP.md, tâche « Spike »).
// Plan B si la fenêtre native pose problème : dessiner dans un sf::RenderTexture et afficher une QImage.
class SfmlWidget : public QWidget {
    Q_OBJECT
public:
    explicit SfmlWidget(QWidget* parent = nullptr);

    // Appelée ~60 fois par seconde : dessinez dans la fenêtre. clear() et display() sont faits pour vous.
    std::function<void(sf::RenderWindow&)> onFrame;
    sf::RenderWindow* window() { return m_window ? &*m_window : nullptr; }

protected:
    QPaintEngine* paintEngine() const override { return nullptr; }   // SFML dessine, pas Qt
    void paintEvent(QPaintEvent*) override {}
    void showEvent(QShowEvent*) override;

private:
    void renderFrame();
    std::optional<sf::RenderWindow> m_window;
    QTimer m_timer;
};
