#include "qt_sfml_input.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

namespace QtSfml {

using Key = sf::Keyboard::Key;

sf::Keyboard::Key toSfmlKey(const QKeyEvent* e) {
    const int k = e->key();
    const bool keypad = e->modifiers() & Qt::KeypadModifier;

    if (k >= Qt::Key_A && k <= Qt::Key_Z) return Key(int(Key::A) + (k - Qt::Key_A));
    if (k >= Qt::Key_0 && k <= Qt::Key_9)
        return keypad ? Key(int(Key::Numpad0) + (k - Qt::Key_0)) : Key(int(Key::Num0) + (k - Qt::Key_0));
    if (k >= Qt::Key_F1 && k <= Qt::Key_F15) return Key(int(Key::F1) + (k - Qt::Key_F1));

#ifdef Q_OS_WIN
    // AZERTY : la rangée des chiffres donne & é " ' ... côté Qt ; SFML (touche virtuelle) y voit Num0..9
    const quint32 vk = e->nativeVirtualKey();
    if (!keypad && vk >= 0x30 && vk <= 0x39) return Key(int(Key::Num0) + int(vk - 0x30));
#endif

    switch (k) {
    case Qt::Key_Escape:      return Key::Escape;
    case Qt::Key_Tab:
    case Qt::Key_Backtab:     return Key::Tab;
    case Qt::Key_Backspace:   return Key::Backspace;
    case Qt::Key_Return:
    case Qt::Key_Enter:       return Key::Enter;
    case Qt::Key_Insert:      return Key::Insert;
    case Qt::Key_Delete:      return Key::Delete;
    case Qt::Key_Pause:       return Key::Pause;
    case Qt::Key_Home:        return Key::Home;
    case Qt::Key_End:         return Key::End;
    case Qt::Key_PageUp:      return Key::PageUp;
    case Qt::Key_PageDown:    return Key::PageDown;
    case Qt::Key_Left:        return Key::Left;
    case Qt::Key_Right:       return Key::Right;
    case Qt::Key_Up:          return Key::Up;
    case Qt::Key_Down:        return Key::Down;
    case Qt::Key_Space:       return Key::Space;
    case Qt::Key_Shift:       return Key::LShift;
    case Qt::Key_Control:     return Key::LControl;
    case Qt::Key_Alt:         return Key::LAlt;
    case Qt::Key_AltGr:       return Key::RAlt;
    case Qt::Key_Meta:        return Key::LSystem;
    case Qt::Key_Menu:        return Key::Menu;
    case Qt::Key_Plus:        return keypad ? Key::Add : Key::Equal;
    case Qt::Key_Minus:       return keypad ? Key::Subtract : Key::Hyphen;
    case Qt::Key_Asterisk:    return keypad ? Key::Multiply : Key::Unknown;
    case Qt::Key_Slash:       return keypad ? Key::Divide : Key::Slash;
    case Qt::Key_Equal:       return Key::Equal;
    case Qt::Key_BracketLeft: return Key::LBracket;
    case Qt::Key_BracketRight:return Key::RBracket;
    case Qt::Key_Semicolon:   return Key::Semicolon;
    case Qt::Key_Comma:       return Key::Comma;
    case Qt::Key_Period:      return Key::Period;
    case Qt::Key_Apostrophe:  return Key::Apostrophe;
    case Qt::Key_Backslash:   return Key::Backslash;
    case Qt::Key_QuoteLeft:   return Key::Grave;
    default:                  return Key::Unknown;
    }
}

namespace {
std::optional<sf::Mouse::Button> toSfmlButton(Qt::MouseButton b) {
    switch (b) {
    case Qt::LeftButton:   return sf::Mouse::Button::Left;
    case Qt::RightButton:  return sf::Mouse::Button::Right;
    case Qt::MiddleButton: return sf::Mouse::Button::Middle;
    case Qt::BackButton:   return sf::Mouse::Button::Extra1;
    case Qt::ForwardButton:return sf::Mouse::Button::Extra2;
    default:               return std::nullopt;
    }
}
sf::Vector2i toPixels(QPointF p, double dpr) { return { int(p.x() * dpr), int(p.y() * dpr) }; }
} // namespace

std::optional<sf::Event> toSfmlEvent(const QEvent* event, double dpr) {
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::KeyRelease: {
        const auto* e = static_cast<const QKeyEvent*>(event);
        if (e->isAutoRepeat()) return std::nullopt;   // SFML garde la touche enfoncée, pas besoin des répétitions
        const Key key = toSfmlKey(e);
        if (key == Key::Unknown) return std::nullopt;
        const Qt::KeyboardModifiers m = e->modifiers();
        const bool alt = m & Qt::AltModifier, ctrl = m & Qt::ControlModifier;
        const bool shift = m & Qt::ShiftModifier, sys = m & Qt::MetaModifier;
        if (event->type() == QEvent::KeyPress)
            return sf::Event(sf::Event::KeyPressed{ key, sf::Keyboard::Scancode::Unknown, alt, ctrl, shift, sys });
        return sf::Event(sf::Event::KeyReleased{ key, sf::Keyboard::Scancode::Unknown, alt, ctrl, shift, sys });
    }
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonRelease: {
        const auto* e = static_cast<const QMouseEvent*>(event);
        const auto button = toSfmlButton(e->button());
        if (!button) return std::nullopt;
        const sf::Vector2i pos = toPixels(e->position(), dpr);
        if (event->type() == QEvent::MouseButtonRelease) return sf::Event(sf::Event::MouseButtonReleased{ *button, pos });
        return sf::Event(sf::Event::MouseButtonPressed{ *button, pos });
    }
    case QEvent::MouseMove: {
        const auto* e = static_cast<const QMouseEvent*>(event);
        return sf::Event(sf::Event::MouseMoved{ toPixels(e->position(), dpr) });
    }
    case QEvent::Wheel: {
        const auto* e = static_cast<const QWheelEvent*>(event);
        const float notches = e->angleDelta().y() / 120.f;
        if (notches == 0.f) return std::nullopt;
        return sf::Event(sf::Event::MouseWheelScrolled{ sf::Mouse::Wheel::Vertical, notches, toPixels(e->position(), dpr) });
    }
    case QEvent::FocusIn:  return sf::Event(sf::Event::FocusGained{});
    case QEvent::FocusOut: return sf::Event(sf::Event::FocusLost{});
    default:               return std::nullopt;
    }
}

} // namespace QtSfml
