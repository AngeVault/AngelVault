#include "sfml_widget.h"
#include <type_traits>

/**
 * @file sfml_widget.cpp
 * @brief SFML-backed Qt widget implementation.
 *
 * This file embeds a native SFML render window inside a Qt widget. The widget
 * is responsible for:
 * - creating the native SFML window on first show,
 * - synchronizing the SFML viewport to the Qt widget size,
 * - polling and handling resize events,
 * - invoking custom init/frame callbacks for rendering logic.
 */

namespace {
/**
 * Converts a Qt window identifier to the native handle type expected by SFML.
 *
 * On Windows, sf::WindowHandle is usually a HWND pointer-like handle. On Linux/X11,
 * it is an integer. This helper preserves the correct type for both platforms
 * without scattering platform-specific code inside the widget implementation.
 *
 * @tparam H The native handle type used by SFML.
 * @param id The Qt window identifier (WId).
 * @return The equivalent native handle usable by sf::Window.
 */
template <typename H>
H toNativeHandle(WId id) {
    if constexpr (std::is_pointer_v<H>) return reinterpret_cast<H>(id);
    else return static_cast<H>(id);
}
}

/**
 * Creates an SFML-rendering Qt widget.
 *
 * The widget is configured as a native, paint-on-screen container that draws
 * directly to a native window instead of using Qt’s software rendering path.
 * A periodic QTimer keeps the SFML content refreshed.
 *
 * @param parent Parent Qt widget, if any.
 */
SfmlWidget::SfmlWidget(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_DontCreateNativeAncestors);   // only the SFML view is native; otherwise resize/drag is broken
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);

    connect(&m_timer, &QTimer::timeout, this, [this] { renderFrame(); });
    m_timer.start(16);
}

/**
 * Called when the widget is shown.
 *
 * The first time the widget becomes visible, a native SFML window is created
 * using the Qt window handle. Once created, the widget size is synchronized
 * with SFML and the optional initialization callback is invoked.
 *
 * @param e Show event information.
 */
void SfmlWidget::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);

    if (!m_window) {
        m_window.emplace(toNativeHandle<sf::WindowHandle>(winId()));
        syncSize();

        if (onInit) onInit(*m_window);
    }
}

/**
 * Called whenever the widget is resized.
 *
 * The Qt size is synchronized with the embedded SFML view and a redraw is
 * requested immediately.
 *
 * @param e Resize event information.
 */
void SfmlWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    syncSize();
    renderFrame();
}

/**
 * Synchronizes the SFML window size with the Qt widget size.
 *
 * This ensures the render area matches the widget’s current size, taking into
 * account device pixel ratio (HiDPI / Retina support). A view is also reset so
 * the content is not stretched.
 */
void SfmlWidget::syncSize() {
    if (!m_window) return;

    const qreal dpr = devicePixelRatioF();
    const sf::Vector2u size(
        unsigned(qMax(1, qRound(width() * dpr))),
        unsigned(qMax(1, qRound(height() * dpr)))
    );

    if (m_window->getSize() != size) {
        m_window->setSize(size);
        m_window->setView(sf::View(sf::FloatRect({ 0.f, 0.f }, sf::Vector2f(size))));
    }
}

/**
 * Renders a single frame if the widget is visible and initialized.
 *
 * The method:
 * - ignores rendering when the widget is not visible,
 * - handles SFML resize events by updating the view to the new dimensions,
 * - clears the back buffer,
 * - invokes the custom frame callback,
 * - displays the rendered frame.
 */
void SfmlWidget::renderFrame() {
    if (!m_window || !isVisible()) return;   // hidden view (e.g., under the game view): nothing to draw

    // Keep the internal SFML size synchronized and adjust the view when resized
    while (const std::optional event = m_window->pollEvent()) {
        if (const auto* resized = event->getIf<sf::Event::Resized>())
            m_window->setView(sf::View(sf::FloatRect({ 0.f, 0.f }, sf::Vector2f(resized->size))));
    }

    m_window->clear(sf::Color(uint8_t(m_clearColor.red()), uint8_t(m_clearColor.green()), uint8_t(m_clearColor.blue())));

    if (onFrame) onFrame(*m_window);

    m_window->display();
}