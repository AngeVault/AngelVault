#pragma once
// Traduction des événements Qt en événements SFML.
// Dans l'éditeur, le jeu tourne dans un widget Qt : c'est Qt qui reçoit clavier / souris,
// on les convertit pour l'InputManager du jeu (même chemin que dans rpg.exe).
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <optional>

class QEvent;
class QKeyEvent;

namespace QtSfml {

// Touche Qt -> touche SFML (Unknown si pas d'équivalent)
sf::Keyboard::Key toSfmlKey(const QKeyEvent* event);

// Événement Qt (clavier, souris, molette, focus) -> événement SFML. `dpr` : facteur d'échelle de l'écran.
std::optional<sf::Event> toSfmlEvent(const QEvent* event, double dpr);

} // namespace QtSfml
