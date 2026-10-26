#include "sfml_widget.h"
#include <type_traits>

namespace {
// sf::WindowHandle : HWND (pointeur) sous Windows, entier (Window X11) sous Linux
template <typename H>
H toNativeHandle(WId id) {
    if constexpr (std::is_pointer_v<H>) return reinterpret_cast<H>(id);
    else return static_cast<H>(id);
}
}

SfmlWidget::SfmlWidget(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_DontCreateNativeAncestors);   // seule la vue SFML est native (sinon la fenêtre perd le resize/drag)
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    connect(&m_timer, &QTimer::timeout, this, [this] { renderFrame(); });
    m_timer.start(16);
}

void SfmlWidget::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!m_window) {
        m_window.emplace(toNativeHandle<sf::WindowHandle>(winId()));
        syncSize();
        if (onInit) onInit(*m_window);
    }
}

void SfmlWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    syncSize();
    renderFrame();
}

void SfmlWidget::syncSize() {
    if (!m_window) return;
    const qreal dpr = devicePixelRatioF();
    const sf::Vector2u size(unsigned(qMax(1, qRound(width() * dpr))), unsigned(qMax(1, qRound(height() * dpr))));
    if (m_window->getSize() != size) {
        m_window->setSize(size);
        m_window->setView(sf::View(sf::FloatRect({ 0.f, 0.f }, sf::Vector2f(size))));
    }
}

void SfmlWidget::renderFrame() {
    if (!m_window || !isVisible()) return;   // vue cachée (ex. sous la vue de jeu) : rien à dessiner
    // Garde la taille interne de SFML à jour et recadre la vue (sinon tout est étiré)
    while (const std::optional event = m_window->pollEvent()) {
        if (const auto* resized = event->getIf<sf::Event::Resized>())
            m_window->setView(sf::View(sf::FloatRect({ 0.f, 0.f }, sf::Vector2f(resized->size))));
    }
    m_window->clear(sf::Color(27, 28, 31));
    if (onFrame) onFrame(*m_window);
    m_window->display();
}
