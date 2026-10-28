#pragma once
// Accès direct aux entrées depuis n'importe quel système ou script, comme `Input.GetAxis` d'Unity :
//
//     #include "input/input.h"
//     float x = Input::getAxis("Horizontal");
//     if (Input::getButtonDown("Jump")) ...
//
// Utilise l'InputManager du Game en cours (InputManager::current()).
// Sans Game (tests...), tout renvoie 0 / false.
#include "input/input_manager.h"

namespace Input {

inline float getAxis(const std::string& action) {
    const InputManager* m = InputManager::current(); return m ? m->getAxis(action) : 0.f;
}
inline bool getButton(const std::string& action) {
    const InputManager* m = InputManager::current(); return m && m->getButton(action);
}
inline bool getButtonDown(const std::string& action) {
    const InputManager* m = InputManager::current(); return m && m->getButtonDown(action);
}
inline bool getButtonUp(const std::string& action) {
    const InputManager* m = InputManager::current(); return m && m->getButtonUp(action);
}
inline bool getKey(sf::Keyboard::Key key) {
    const InputManager* m = InputManager::current(); return m && m->getKey(key);
}
inline bool getKeyDown(sf::Keyboard::Key key) {
    const InputManager* m = InputManager::current(); return m && m->getKeyDown(key);
}
inline bool getMouseButton(sf::Mouse::Button button) {
    const InputManager* m = InputManager::current(); return m && m->getMouseButton(button);
}
inline sf::Vector2i mousePosition() {
    const InputManager* m = InputManager::current(); return m ? m->mousePosition() : sf::Vector2i{};
}
inline float mouseWheelDelta() {
    const InputManager* m = InputManager::current(); return m ? m->mouseWheelDelta() : 0.f;
}

} // namespace Input
