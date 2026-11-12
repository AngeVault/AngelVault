#include "sfml_widget.h"

#include <QImage>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <algorithm>
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
    drawBackground();

    if (onFrame) onFrame(*m_window);

    m_window->display();
}
void SfmlWidget::setBackgroundImage(const QString& path) {
    const QString p = path.trimmed() == "none" ? QString() : path.trimmed();
    if (p == m_backgroundPath) return;
    m_backgroundPath = p;
    m_backgroundDirty = true;
}

// Image de fond du thème : dessinée en coordonnées écran, en « cover » (remplit la vue, centrée, sans déformer),
// avant la scène. La vue de la scène est remise telle quelle ensuite.
void SfmlWidget::drawBackground() {
    if (m_backgroundDirty) {
        m_backgroundDirty = false;
        m_background.reset();
        if (!m_backgroundPath.isEmpty()) {
            const QImage image = QImage(m_backgroundPath).convertToFormat(QImage::Format_RGBA8888);
            if (!image.isNull()) {
                const sf::Image pixels(sf::Vector2u(unsigned(image.width()), unsigned(image.height())), image.constBits());
                sf::Texture texture;
                if (texture.loadFromImage(pixels)) {
                    texture.setSmooth(true);
                    m_background = std::move(texture);
                }
            }
        }
    }
    if (!m_background || m_backgroundOpacity <= 0.0) return;

    const sf::Vector2f target(m_window->getSize());
    const sf::Vector2f size(m_background->getSize());
    if (target.x <= 0.f || target.y <= 0.f) return;
    const float scale = std::max(target.x / size.x, target.y / size.y);
    sf::Sprite sprite(*m_background);
    sprite.setScale({ scale, scale });
    sprite.setPosition({ (target.x - size.x * scale) * 0.5f, (target.y - size.y * scale) * 0.5f });
    sprite.setColor(sf::Color(255, 255, 255, uint8_t(m_backgroundOpacity * 255.0)));

    const sf::View sceneView = m_window->getView();
    m_window->setView(sf::View(sf::FloatRect({ 0.f, 0.f }, target)));
    m_window->draw(sprite);
    m_window->setView(sceneView);
}
