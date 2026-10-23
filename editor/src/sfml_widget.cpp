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
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    connect(&m_timer, &QTimer::timeout, this, [this] { renderFrame(); });
    m_timer.start(16);
}

void SfmlWidget::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!m_window) m_window.emplace(toNativeHandle<sf::WindowHandle>(winId()));
}

void SfmlWidget::renderFrame() {
    if (!m_window) return;
    while (m_window->pollEvent()) {}          // garde la taille interne de SFML à jour
    m_window->clear(sf::Color(27, 28, 31));
    if (onFrame) onFrame(*m_window);
    m_window->display();
}
