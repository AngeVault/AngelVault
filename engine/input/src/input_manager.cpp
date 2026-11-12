#include "input/input_manager.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace {

/**
 * @brief Tableau de correspondance entre codes clavier SFML et libellés lisibles.
 *
 * Les noms sont ordonnés selon l'énumération sf::Keyboard::Key, donc l'index 0
 * correspond à la touche "A" et l'index KeyCount correspond à la fin de l'énumération.
 * Cela permet de convertir facilement un code clavier en texte et inversement.
 */
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

/**
 * @brief Noms des boutons souris reconnus par le système.
 */
const char* const kMouseNames[] = { "Left", "Right", "Middle", "Extra1", "Extra2" };

/**
 * @brief Noms des axes de manette reconnus.
 */
const char* const kAxisNames[] = { "X", "Y", "Z", "R", "U", "V", "PovX", "PovY" };

/**
 * @brief Pointeur vers le gestionnaire d'entrée courant.
 *
 * Permet d'accéder globalement à l'InputManager actif depuis des points du projet
 * qui n'ont pas directement accès à l'instance.
 */
InputManager* g_current = nullptr;

/**
 * @brief Supprime les espaces de début et de fin d'une chaîne.
 *
 * @param s Chaîne source.
 * @return Chaîne sans espaces en début/fin.
 */
std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

/**
 * @brief Convertit un float en chaîne.
 *
 * Utilisé pour enregistrer les valeurs non entières dans les fichiers de configuration.
 *
 * @param v Valeur à convertir.
 * @return Représentation textuelle.
 */
std::string formatFloat(float v) {
    std::ostringstream o;
    o << v;
    return o.str();
}

} // namespace

// ---- Configuration ---------------------------------------------------------------------

/**
 * @brief Ajoute une action d'entrée ou la met à jour si elle existe déjà.
 *
 * Les actions représentent des noms logiques comme "Move", "Jump" ou "Shoot".
 * Chaque action peut être de type bouton ou axe.
 *
 * @param name Nom de l'action.
 * @param type Type logique de l'action.
 * @return Référence vers l'action créée ou mise à jour.
 */
InputAction& InputManager::addAction(const std::string& name, InputAction::Type type) {
    if (InputAction* existing = findAction(name)) { existing->type = type; return *existing; }
    m_actions.push_back({ name, type, 0.2f, {} });
    m_state.resize(m_actions.size());
    return m_actions.back();
}

/**
 * @brief Recherche une action par son nom.
 *
 * @param name Nom de l'action recherchée.
 * @return Pointeur vers l'action si elle existe, nullptr sinon.
 */
InputAction* InputManager::findAction(const std::string& name) {
    for (InputAction& a : m_actions) if (a.name == name) return &a;
    return nullptr;
}

/**
 * @brief Recherche une action par son nom en mode lecture seule.
 *
 * @param name Nom de l'action recherchée.
 * @return Pointeur constant vers l'action si elle existe, nullptr sinon.
 */
const InputAction* InputManager::findAction(const std::string& name) const {
    for (const InputAction& a : m_actions) if (a.name == name) return &a;
    return nullptr;
}

/**
 * @brief Supprime une action d'entrée et son état associé.
 *
 * @param name Nom de l'action à enlever.
 */
void InputManager::removeAction(const std::string& name) {
    for (std::size_t i = 0; i < m_actions.size(); ++i)
        if (m_actions[i].name == name) {
            m_actions.erase(m_actions.begin() + std::ptrdiff_t(i));
            if (i < m_state.size()) m_state.erase(m_state.begin() + std::ptrdiff_t(i));
            return;
        }
}

/**
 * @brief Charge les actions d'entrée depuis un fichier.
 *
 * @param path Chemin du fichier à lire.
 * @return true si le fichier a été lu correctement, false sinon.
 */
bool InputManager::loadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    return loadFromString(ss.str());
}

/**
 * @brief Écrit les actions d'entrée dans un fichier.
 *
 * @param path Chemin du fichier de destination.
 * @return true si l'écriture a réussi, false sinon.
 */
bool InputManager::saveToFile(const std::string& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << toString();
    return bool(out);
}

/**
 * @brief Parse une configuration texte de bindings d'entrée.
 *
 * Le fichier est structuré en deux parties :
 * - une section "[NomAction] axis|button deadzone=..." pour configurer l'action
 * - des lignes de binding suivantes, par exemple "key W", "mouse Left", "joyaxis 0 X"
 *
 * Les lignes vides et les commentaires commençant par # sont ignorés.
 *
 * @param text Contenu texte de configuration.
 * @return true si le parsing s'est bien déroulé, false sinon.
 */
bool InputManager::loadFromString(const std::string& text) {
    std::vector<InputAction> actions;
    std::istringstream in(text);
    std::string line;

    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (line[0] == '[') { // [Nom] axis deadzone=0.2
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

        if (actions.empty()) continue; // liaison avant toute action : ignorée

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

/**
 * @brief Sérialise toutes les actions d'entrée dans un format lisible.
 *
 * @return Chaîne texte de configuration.
 */
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

/**
 * @brief Traite un événement SFML et mémorise les états de clavier/souris.
 *
 * Les états internes de l'InputManager sont mis à jour pour distinguer :
 * - touches actuellement maintenues
 * - touches pressées durant cette frame
 * - touches relâchées durant cette frame
 *
 * @param event Événement SFML à analyser.
 */
void InputManager::handleEvent(const sf::Event& event) {
    if (const auto* e = event.getIf<sf::Event::KeyPressed>()) {
        const int k = int(e->code);
        if (k >= 0 && k < int(kKeys)) {
            if (!m_keys[k]) m_keysDown[k] = true; // pas la répétition automatique
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

/**
 * @brief Calcule la valeur effective d'une liaison d'entrée.
 *
 * @param b Liaison à évaluer.
 * @param deadZone Zone morte appliquée aux axes.
 * @return Valeur normalisée de la liaison.
 */
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

/**
 * @brief Met à jour tous les états d'action calculés à partir des liaisons actives.
 *
 * Cette méthode agrège plusieurs entrées dans une valeur unique par action.
 * Pour les axes, la valeur est bornée entre -1 et 1, puis la molette est ajoutée.
 *
 * Les états `pressed`, `released` et `down` servent à détecter les transitions de frame.
 */
void InputManager::update() {
    m_state.resize(m_actions.size());
    for (std::size_t i = 0; i < m_actions.size(); ++i) {
        const InputAction& a = m_actions[i];
        ActionState& s = m_state[i];
        float held = 0.f, wheel = 0.f;
        bool tapped = false; // enfoncé puis relâché dans la même frame : compte quand même
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

/**
 * @brief Remet à zéro les évènements de pression/relâchement de la frame courante.
 */
void InputManager::endFrame() {
    m_keysDown.fill(false);
    m_keysUp.fill(false);
    m_mouseDown.fill(false);
    m_mouseUp.fill(false);
    m_wheel = 0.f;
}

/**
 * @brief Vide totalement l'état clavier/souris et applique un reset de frame.
 *
 * Utilisé lors de la perte de focus de la fenêtre.
 */
void InputManager::clearState() {
    m_keys.fill(false);
    m_mouse.fill(false);
    endFrame();
}

// ---- Lecture -----------------------------------------------------------------------------

/**
 * @brief Retourne l'état courant d'une action par son nom.
 *
 * @param action Nom de l'action.
 * @return Pointeur vers l'état, ou nullptr si l'action n'existe pas.
 */
const InputManager::ActionState* InputManager::state(const std::string& action) const {
    for (std::size_t i = 0; i < m_actions.size() && i < m_state.size(); ++i)
        if (m_actions[i].name == action) return &m_state[i];
    return nullptr;
}

/**
 * @brief Retourne la valeur numérique d'un axe.
 *
 * @param action Nom de l'action.
 * @return Valeur de l'axe, typiquement comprise entre -1 et 1.
 */
float InputManager::getAxis(const std::string& action) const {
    const ActionState* s = state(action); return s ? s->value : 0.f;
}

/**
 * @brief Retourne si un bouton est actuellement enfoncé.
 *
 * @param action Nom de l'action.
 * @return true si l'action est active.
 */
bool InputManager::getButton(const std::string& action) const {
    const ActionState* s = state(action); return s && s->down;
}

/**
 * @brief Retourne si un bouton vient d'être pressé.
 *
 * @param action Nom de l'action.
 * @return true lors de la frame où l'action tombe en pression.
 */
bool InputManager::getButtonDown(const std::string& action) const {
    const ActionState* s = state(action); return s && s->pressed;
}

/**
 * @brief Retourne si un bouton vient d'être relâché.
 *
 * @param action Nom de l'action.
 * @return true lors de la frame où l'action tombe en relâchement.
 */
bool InputManager::getButtonUp(const std::string& action) const {
    const ActionState* s = state(action); return s && s->released;
}

/**
 * @brief Vérifie si une touche clavier est actuellement appuyée.
 *
 * @param key Touche SFML.
 * @return true si la touche est active.
 */
bool InputManager::getKey(sf::Keyboard::Key key) const {
    const int k = int(key); return k >= 0 && k < int(kKeys) && m_keys[k];
}

/**
 * @brief Vérifie si une touche clavier vient d'être enfoncée.
 *
 * @param key Touche SFML.
 * @return true si la touche a été pressée pendant la frame.
 */
bool InputManager::getKeyDown(sf::Keyboard::Key key) const {
    const int k = int(key); return k >= 0 && k < int(kKeys) && m_keysDown[k];
}

/**
 * @brief Vérifie si une touche clavier vient d'être relâchée.
 *
 * @param key Touche SFML.
 * @return true si la touche a été relâchée pendant la frame.
 */
bool InputManager::getKeyUp(sf::Keyboard::Key key) const {
    const int k = int(key); return k >= 0 && k < int(kKeys) && m_keysUp[k];
}

/**
 * @brief Vérifie si un bouton souris est actuellement appuyé.
 *
 * @param button Bouton SFML.
 * @return true si le bouton est actif.
 */
bool InputManager::getMouseButton(sf::Mouse::Button button) const {
    const int b = int(button); return b >= 0 && b < int(kButtons) && m_mouse[b];
}

/**
 * @brief Vérifie si un bouton souris vient d'être pressé.
 *
 * @param button Bouton SFML.
 * @return true si le bouton a été pressé pendant la frame.
 */
bool InputManager::getMouseButtonDown(sf::Mouse::Button button) const {
    const int b = int(button); return b >= 0 && b < int(kButtons) && m_mouseDown[b];
}

// ---- Noms ----------------------------------------------------------------------------------

/**
 * @brief Convertit une touche clavier SFML en nom lisible.
 *
 * @param key Touche à convertir.
 * @return Nom de la touche, ou "Unknown" si invalide.
 */
std::string InputManager::keyName(sf::Keyboard::Key key) {
    const int k = int(key);
    return (k >= 0 && k < int(kKeys)) ? kKeyNames[k] : "Unknown";
}

/**
 * @brief Convertit un nom de touche en code SFML.
 *
 * @param name Nom de la touche.
 * @return Code clavier associé, ou Unknown si le nom est inconnu.
 */
sf::Keyboard::Key InputManager::keyFromName(const std::string& name) {
    for (int k = 0; k < int(kKeys); ++k)
        if (name == kKeyNames[k]) return sf::Keyboard::Key(k);
    return sf::Keyboard::Key::Unknown;
}

/**
 * @brief Convertit un bouton souris en nom lisible.
 *
 * @param button Index du bouton.
 * @return Nom du bouton, ou "Left" par défaut si invalide.
 */
std::string InputManager::mouseButtonName(int button) {
    return (button >= 0 && button < int(std::size(kMouseNames))) ? kMouseNames[button] : "Left";
}

/**
 * @brief Convertit un nom de bouton souris en index.
 *
 * @param name Nom du bouton.
 * @return Index du bouton ou -1 si inconnu.
 */
int InputManager::mouseButtonFromName(const std::string& name) {
    for (int i = 0; i < int(std::size(kMouseNames)); ++i) if (name == kMouseNames[i]) return i;
    return -1;
}

/**
 * @brief Convertit un axe de manette en nom lisible.
 *
 * @param axis Index de l'axe.
 * @return Nom de l'axe, ou "X" par défaut si invalide.
 */
std::string InputManager::joystickAxisName(int axis) {
    return (axis >= 0 && axis < int(std::size(kAxisNames))) ? kAxisNames[axis] : "X";
}

/**
 * @brief Convertit un nom d'axe de manette en index.
 *
 * @param name Nom de l'axe.
 * @return Index de l'axe ou -1 si inconnu.
 */
int InputManager::joystickAxisFromName(const std::string& name) {
    for (int i = 0; i < int(std::size(kAxisNames)); ++i) if (name == kAxisNames[i]) return i;
    return -1;
}

/**
 * @brief Retourne le gestionnaire d'entrée actif.
 *
 * @return Pointeur vers l'InputManager courant.
 */
InputManager* InputManager::current() { return g_current; }

/**
 * @brief Définit le gestionnaire d'entrée actif.
 *
 * @param manager Nouveau gestionnaire courant.
 */
void InputManager::setCurrent(InputManager* manager) { g_current = manager; }