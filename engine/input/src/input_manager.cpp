#include "input/input_manager.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace {

// Noms des touches : identifiants de sf::Keyboard::Key, dans l'ordre de l'enum (A = 0).
const char* const kKeyNames[] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
    "U", "V", "W", "X", "Y", "Z",
    "Num0", "Num1", "Num2", "Num3", "Num4", "Num5", "Num6", "Num7", "Num8", "Num9",
    "Escape", "LControl", "LShift", "LAlt", "LSystem", "RControl", "RShift", "RAlt", "RSystem", "Menu",
    "LBracket", "RBracket", "Semicolon", "Comma", "Period", "Apostrophe", "Slash", "Backslash", "Grave",
    "Equal", "Hyphen", "Space", "Enter", "Backspace", "Tab", "PageUp", "PageDown", "End", "Home",
    "Insert", "Delete", "Add", "Subtract", "Multiply", "Divide", "Left", "Right", "Up", "Down",
    "Numpad0", "Numpad1", "Numpad2", "Numpad3", "Numpad4", "Numpad5", "Numpad6", "Numpad7", "Numpad8", "Numpad9",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "F13", "F14", "F15", "Pause",
};
static_assert(std::size(kKeyNames) == sf::Keyboard::KeyCount, "kKeyNames doit suivre sf::Keyboard::Key");

const char* const kMouseNames[] = { "Left", "Right", "Middle", "Extra1", "Extra2" };
const char* const kAxisNames[] = { "X", "Y", "Z", "R", "U", "V", "PovX", "PovY" };

InputManager* g_current = nullptr;

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string formatFloat(float v) {
    std::ostringstream o;
    o << v;
    return o.str();
}

} // namespace

// ---- Configuration ---------------------------------------------------------------------

InputAction& InputManager::addAction(const std::string& name, InputAction::Type type) {
    if (InputAction* existing = findAction(name)) { existing->type = type; return *existing; }
    m_actions.push_back({ name, type, 0.2f, {} });
    m_state.resize(m_actions.size());
    return m_actions.back();
}

InputAction* InputManager::findAction(const std::string& name) {
    for (InputAction& a : m_actions) if (a.name == name) return &a;
    return nullptr;
}

const InputAction* InputManager::findAction(const std::string& name) const {
    for (const InputAction& a : m_actions) if (a.name == name) return &a;
    return nullptr;
}

void InputManager::removeAction(const std::string& name) {
    for (std::size_t i = 0; i < m_actions.size(); ++i)
        if (m_actions[i].name == name) {
            m_actions.erase(m_actions.begin() + std::ptrdiff_t(i));
            if (i < m_state.size()) m_state.erase(m_state.begin() + std::ptrdiff_t(i));
            return;
        }
}

bool InputManager::loadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    return loadFromString(ss.str());
}

bool InputManager::saveToFile(const std::string& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << toString();
    return bool(out);
}

bool InputManager::loadFromString(const std::string& text) {
    std::vector<InputAction> actions;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (line[0] == '[') {   // [Nom] axis deadzone=0.2
            const auto close = line.find(']');
            if (close == std::string::npos) continue;
            InputAction a;
            a.name = trim(line.substr(1, close - 1));
            std::istringstream rest(line.substr(close + 1));
            std::string word;
            while (rest >> word) {
                if (word == "axis") a.type = InputAction::Type::Axis;
                else if (word == "button") a.type = InputAction::Type::Button;
                else if (word.rfind("deadzone=", 0) == 0) {
                    try { a.deadZone = std::clamp(std::stof(word.substr(9)), 0.f, 0.99f); } catch (...) {}
                }
            }
            if (!a.name.empty()) actions.push_back(a);
            continue;
        }
        if (actions.empty()) continue;   // lien avant toute action : ignoré

        std::istringstream words(line);
        std::string kind;
        words >> kind;
        InputBinding b;
        bool ok = true;
        if (kind == "key") {
            std::string name; words >> name;
            const sf::Keyboard::Key k = keyFromName(name);
            ok = k != sf::Keyboard::Key::Unknown;
            b.source = InputBinding::Source::Key; b.code = int(k);
        } else if (kind == "mouse") {
            std::string name; words >> name;
            b.source = InputBinding::Source::MouseButton; b.code = mouseButtonFromName(name);
            ok = b.code >= 0;
        } else if (kind == "wheel") {
            b.source = InputBinding::Source::MouseWheel;
        } else if (kind == "joybutton") {
            b.source = InputBinding::Source::JoystickButton;
            ok = bool(words >> b.joystick >> b.code);
        } else if (kind == "joyaxis") {
            std::string axis;
            b.source = InputBinding::Source::JoystickAxis;
            ok = bool(words >> b.joystick >> axis);
            b.code = joystickAxisFromName(axis);
            ok = ok && b.code >= 0;
        } else {
            ok = false;
        }
        float scale;
        if (words >> scale) b.scale = scale;
        if (ok) actions.back().bindings.push_back(b);
    }
    m_actions = std::move(actions);
    m_state.assign(m_actions.size(), {});
    return true;
}

std::string InputManager::toString() const {
    std::ostringstream o;
    o << "# AngeVault : actions d'entree (Edition > Input Manager... dans l'editeur)\n";
    o << "# [Nom] button|axis deadzone=<0..1>\n";
    o << "#   key <Touche> [echelle] | mouse <Left|Right|Middle|Extra1|Extra2> [echelle] | wheel [echelle]\n";
    o << "#   joybutton <manette> <bouton> [echelle] | joyaxis <manette> <X|Y|Z|R|U|V|PovX|PovY> [echelle]\n";
    for (const InputAction& a : m_actions) {
        o << "\n[" << a.name << "] " << (a.type == InputAction::Type::Axis ? "axis" : "button");
        if (a.deadZone != 0.2f) o << " deadzone=" << formatFloat(a.deadZone);
        o << "\n";
        for (const InputBinding& b : a.bindings) {
            switch (b.source) {
            case InputBinding::Source::Key:            o << "key " << keyName(sf::Keyboard::Key(b.code)); break;
            case InputBinding::Source::MouseButton:    o << "mouse " << mouseButtonName(b.code); break;
            case InputBinding::Source::MouseWheel:     o << "wheel"; break;
            case InputBinding::Source::JoystickButton: o << "joybutton " << b.joystick << " " << b.code; break;
            case InputBinding::Source::JoystickAxis:   o << "joyaxis " << b.joystick << " " << joystickAxisName(b.code); break;
            }
            if (b.scale != 1.f) o << " " << formatFloat(b.scale);
            o << "\n";
        }
    }
    return o.str();
}

// ---- Boucle de jeu ---------------------------------------------------------------------

void InputManager::handleEvent(const sf::Event& event) {
    if (const auto* e = event.getIf<sf::Event::KeyPressed>()) {
        const int k = int(e->code);
        if (k >= 0 && k < int(kKeys)) {
            if (!m_keys[k]) m_keysDown[k] = true;   // pas la répétition automatique
            m_keys[k] = true;
        }
    } else if (const auto* e = event.getIf<sf::Event::KeyReleased>()) {
        const int k = int(e->code);
        if (k >= 0 && k < int(kKeys)) { m_keys[k] = false; m_keysUp[k] = true; }
    } else if (const auto* e = event.getIf<sf::Event::MouseButtonPressed>()) {
        const int b = int(e->button);
        if (b >= 0 && b < int(kButtons)) { m_mouse[b] = true; m_mouseDown[b] = true; }
        m_mousePosition = e->position;
    } else if (const auto* e = event.getIf<sf::Event::MouseButtonReleased>()) {
        const int b = int(e->button);
        if (b >= 0 && b < int(kButtons)) { m_mouse[b] = false; m_mouseUp[b] = true; }
        m_mousePosition = e->position;
    } else if (const auto* e = event.getIf<sf::Event::MouseMoved>()) {
        m_mousePosition = e->position;
    } else if (const auto* e = event.getIf<sf::Event::MouseWheelScrolled>()) {
        if (e->wheel == sf::Mouse::Wheel::Vertical) m_wheel += e->delta;
    } else if (event.is<sf::Event::FocusLost>()) {
        m_focused = false;
        clearState();
    } else if (event.is<sf::Event::FocusGained>()) {
        m_focused = true;
    }
}

float InputManager::bindingValue(const InputBinding& b, float deadZone) const {
    switch (b.source) {
    case InputBinding::Source::Key: {
        const bool on = b.code >= 0 && b.code < int(kKeys) && (m_keys[b.code] || m_keysDown[b.code]);
        return on ? b.scale : 0.f;
    }
    case InputBinding::Source::MouseButton: {
        const bool on = b.code >= 0 && b.code < int(kButtons) && (m_mouse[b.code] || m_mouseDown[b.code]);
        return on ? b.scale : 0.f;
    }
    case InputBinding::Source::MouseWheel:
        return m_wheel * b.scale;
    case InputBinding::Source::JoystickButton:
        if (!m_focused || !sf::Joystick::isConnected(b.joystick)) return 0.f;
        return sf::Joystick::isButtonPressed(b.joystick, unsigned(b.code)) ? b.scale : 0.f;
    case InputBinding::Source::JoystickAxis: {
        if (!m_focused || !sf::Joystick::isConnected(b.joystick)) return 0.f;
        const auto axis = sf::Joystick::Axis(b.code);
        if (!sf::Joystick::hasAxis(b.joystick, axis)) return 0.f;
        const float v = sf::Joystick::getAxisPosition(b.joystick, axis) / 100.f;
        if (std::abs(v) < deadZone) return 0.f;
        // remet 0..1 après la zone morte, pour un démarrage en douceur
        const float t = (std::abs(v) - deadZone) / std::max(1.f - deadZone, 0.001f);
        return std::copysign(std::min(t, 1.f), v) * b.scale;
    }
    }
    return 0.f;
}

void InputManager::update() {
    m_state.resize(m_actions.size());
    for (std::size_t i = 0; i < m_actions.size(); ++i) {
        const InputAction& a = m_actions[i];
        ActionState& s = m_state[i];
        float held = 0.f, wheel = 0.f;
        bool tapped = false;   // enfoncé puis relâché dans la même frame : compte quand même
        for (const InputBinding& b : a.bindings) {
            const float v = bindingValue(b, a.deadZone);
            if (b.source == InputBinding::Source::MouseWheel) wheel += v;
            else held += v;
            if ((b.source == InputBinding::Source::Key && b.code >= 0 && b.code < int(kKeys) && m_keysDown[b.code]) ||
                (b.source == InputBinding::Source::MouseButton && b.code >= 0 && b.code < int(kButtons) && m_mouseDown[b.code]))
                tapped = true;
        }
        // Axe : liens « maintenus » bornés à -1..1, plus les crans de molette (non bornés)
        s.value = std::clamp(held, -1.f, 1.f) + wheel;
        const bool down = std::abs(s.value) > 0.5f || tapped;
        s.pressed = down && !s.down;
        s.released = !down && s.down;
        s.down = down;
    }
}

void InputManager::endFrame() {
    m_keysDown.fill(false);
    m_keysUp.fill(false);
    m_mouseDown.fill(false);
    m_mouseUp.fill(false);
    m_wheel = 0.f;
}

void InputManager::clearState() {
    m_keys.fill(false);
    m_mouse.fill(false);
    endFrame();
}

// ---- Lecture -----------------------------------------------------------------------------

const InputManager::ActionState* InputManager::state(const std::string& action) const {
    for (std::size_t i = 0; i < m_actions.size() && i < m_state.size(); ++i)
        if (m_actions[i].name == action) return &m_state[i];
    return nullptr;
}

float InputManager::getAxis(const std::string& action) const {
    const ActionState* s = state(action); return s ? s->value : 0.f;
}
bool InputManager::getButton(const std::string& action) const {
    const ActionState* s = state(action); return s && s->down;
}
bool InputManager::getButtonDown(const std::string& action) const {
    const ActionState* s = state(action); return s && s->pressed;
}
bool InputManager::getButtonUp(const std::string& action) const {
    const ActionState* s = state(action); return s && s->released;
}

bool InputManager::getKey(sf::Keyboard::Key key) const {
    const int k = int(key); return k >= 0 && k < int(kKeys) && m_keys[k];
}
bool InputManager::getKeyDown(sf::Keyboard::Key key) const {
    const int k = int(key); return k >= 0 && k < int(kKeys) && m_keysDown[k];
}
bool InputManager::getKeyUp(sf::Keyboard::Key key) const {
    const int k = int(key); return k >= 0 && k < int(kKeys) && m_keysUp[k];
}
bool InputManager::getMouseButton(sf::Mouse::Button button) const {
    const int b = int(button); return b >= 0 && b < int(kButtons) && m_mouse[b];
}
bool InputManager::getMouseButtonDown(sf::Mouse::Button button) const {
    const int b = int(button); return b >= 0 && b < int(kButtons) && m_mouseDown[b];
}

// ---- Noms ----------------------------------------------------------------------------------

std::string InputManager::keyName(sf::Keyboard::Key key) {
    const int k = int(key);
    return (k >= 0 && k < int(kKeys)) ? kKeyNames[k] : "Unknown";
}

sf::Keyboard::Key InputManager::keyFromName(const std::string& name) {
    for (int k = 0; k < int(kKeys); ++k)
        if (name == kKeyNames[k]) return sf::Keyboard::Key(k);
    return sf::Keyboard::Key::Unknown;
}

std::string InputManager::mouseButtonName(int button) {
    return (button >= 0 && button < int(std::size(kMouseNames))) ? kMouseNames[button] : "Left";
}

int InputManager::mouseButtonFromName(const std::string& name) {
    for (int i = 0; i < int(std::size(kMouseNames)); ++i) if (name == kMouseNames[i]) return i;
    return -1;
}

std::string InputManager::joystickAxisName(int axis) {
    return (axis >= 0 && axis < int(std::size(kAxisNames))) ? kAxisNames[axis] : "X";
}

int InputManager::joystickAxisFromName(const std::string& name) {
    for (int i = 0; i < int(std::size(kAxisNames)); ++i) if (name == kAxisNames[i]) return i;
    return -1;
}

InputManager* InputManager::current() { return g_current; }
void InputManager::setCurrent(InputManager* manager) { g_current = manager; }
