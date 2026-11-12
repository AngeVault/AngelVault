#pragma once
// Input Manager du moteur (même idée que l'Input Manager d'Unity).
//
// Le jeu ne lit plus les touches directement : il demande des ACTIONS par leur nom.
//     Input::getAxis("Horizontal")       -> -1..1 (flèches, Q/D, stick gauche...)
//     Input::getButtonDown("Jump")       -> vrai la frame où l'action est enclenchée
//     Input::getAxis("ZoomWheel")        -> crans de molette de cette frame
// Les liens action -> touches sont des DONNÉES : fichier assets/input.cfg du projet,
// modifiable dans l'éditeur (Édition > Input Manager...) sans recompiler.
//
// Alimentation : Game passe chaque sf::Event à handleEvent(), puis update() avant les
// systèmes et endFrame() après (fait automatiquement par Game::tick()).
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Joystick.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <array>
#include <string>
#include <vector>

// Une entrée physique reliée à une action
struct InputBinding {
    enum class Source {
        Key,              // code = sf::Keyboard::Key
        MouseButton,      // code = sf::Mouse::Button
        MouseWheel,       // molette verticale : valeur = crans de la frame
        JoystickButton,   // code = numéro du bouton, joystick = numéro de manette
        JoystickAxis,     // code = sf::Joystick::Axis, valeur = position -1..1 (zone morte appliquée)
    };
    Source source = Source::Key;
    int code = 0;
    float scale = 1.f;          // contribution à l'axe (ex. -1 pour « gauche »)
    unsigned joystick = 0;
};

struct InputAction {
    enum class Type {
        Button,   // enfoncé / relâché
        Axis,     // valeur réelle : somme des liens, bornée à -1..1 (sauf molette : crans)
    };
    std::string name;
    Type type = Type::Button;
    float deadZone = 0.2f;   // manette : en dessous, le stick compte pour 0
    std::vector<InputBinding> bindings;
};

class InputManager {
public:
    // ---- Configuration (les actions) ----------------------------------------------
    InputAction& addAction(const std::string& name, InputAction::Type type = InputAction::Type::Button);
    InputAction* findAction(const std::string& name);
    const InputAction* findAction(const std::string& name) const;
    void removeAction(const std::string& name);
    void clearActions() { m_actions.clear(); m_state.clear(); }
    std::vector<InputAction>& actions() { return m_actions; }
    const std::vector<InputAction>& actions() const { return m_actions; }

    // Petit format texte, une action par bloc :
    //   [Horizontal] axis deadzone=0.2
    //   key Left -1
    //   joyaxis 0 X 1
    bool loadFromFile(const std::string& path);   // false si absent / illisible (actions inchangées)
    bool saveToFile(const std::string& path) const;
    bool loadFromString(const std::string& text);
    std::string toString() const;

    // ---- Boucle de jeu ------------------------------------------------------------
    void handleEvent(const sf::Event& event);   // à appeler pour chaque événement de la fenêtre
    void update();                              // avant les systèmes : calcule l'état des actions
    void endFrame();                            // après les systèmes : oublie les « Down/Up » et la molette
    void clearState();                          // tout relâcher (perte du focus...)

    // ---- Lecture par action (à utiliser dans les systèmes / scripts) ---------------
    float getAxis(const std::string& action) const;
    bool getButton(const std::string& action) const;       // maintenu
    bool getButtonDown(const std::string& action) const;   // enfoncé à cette frame
    bool getButtonUp(const std::string& action) const;     // relâché à cette frame

    // ---- Lecture brute --------------------------------------------------------------
    bool getKey(sf::Keyboard::Key key) const;
    bool getKeyDown(sf::Keyboard::Key key) const;
    bool getKeyUp(sf::Keyboard::Key key) const;
    bool getMouseButton(sf::Mouse::Button button) const;
    bool getMouseButtonDown(sf::Mouse::Button button) const;
    sf::Vector2i mousePosition() const { return m_mousePosition; }   // pixels, dans la fenêtre du jeu
    float mouseWheelDelta() const { return m_wheel; }                 // crans de cette frame

    // ---- Noms (fichier de config, éditeur) ----------------------------------------
    static std::string keyName(sf::Keyboard::Key key);
    static sf::Keyboard::Key keyFromName(const std::string& name);   // Unknown si inconnu
    static std::string mouseButtonName(int button);
    static std::string joystickAxisName(int axis);
    static int mouseButtonFromName(const std::string& name);         // -1 si inconnu
    static int joystickAxisFromName(const std::string& name);        // -1 si inconnu

    // ---- Instance courante (pour la façade Input::) ----------------------------------
    static InputManager* current();
    static void setCurrent(InputManager* manager);

private:
    struct ActionState { float value = 0.f; bool down = false, pressed = false, released = false; };
    float bindingValue(const InputBinding& b, float deadZone) const;
    const ActionState* state(const std::string& action) const;

    std::vector<InputAction> m_actions;
    std::vector<ActionState> m_state;   // même ordre que m_actions

    static constexpr std::size_t kKeys = sf::Keyboard::KeyCount;
    static constexpr std::size_t kButtons = sf::Mouse::ButtonCount;
    std::array<bool, kKeys> m_keys{}, m_keysDown{}, m_keysUp{};
    std::array<bool, kButtons> m_mouse{}, m_mouseDown{}, m_mouseUp{};
    sf::Vector2i m_mousePosition{ 0, 0 };
    float m_wheel = 0.f;          // crans reçus depuis le dernier endFrame()
    bool m_focused = true;
};
