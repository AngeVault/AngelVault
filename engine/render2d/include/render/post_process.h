#pragma once
// post-process : un shader .frag appliqué à toute l'image du jeu, après le rendu des systèmes
//
// le shader reçoit
//     uniform sampler2D frame;       // l'image du jeu
//     uniform vec2 resolution;       // taille en pixels
//     uniform float time;            // secondes depuis le chargement
// et ses propres réglages, lus par l'éditeur pour afficher les curseurs
//     uniform float contrast; // @param <défaut> <min> <max>
//
// les valeurs choisies sont dans assets/settings.cfg (shader.<nom> = valeur)
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Clock.hpp>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

class PostProcess {
public:
    struct Param {
        std::string name;
        float value = 0.f;
        float min = 0.f;
        float max = 1.f;
    };

    // chemin vide = pas de post-process, false si le fichier manque ou ne compile pas (désactivé)
    bool load(const std::string& fragPath, std::string* error = nullptr);
    void clear();
    bool enabled() const { return m_shader.has_value(); }

    // noms inconnus du shader ignorés
    void setParams(const std::map<std::string, float>& values);
    const std::vector<Param>& params() const { return m_params; }

    // copie ce qui est dessiné dans la fenêtre puis le redessine à travers le shader
    void apply(sf::RenderWindow& window);

    // réglages @param d'un source GLSL, avec leur valeur par défaut
    static std::vector<Param> parseParams(const std::string& source);

private:
    std::optional<sf::Shader> m_shader;
    sf::Texture m_frame;
    std::vector<Param> m_params;
    std::set<std::string> m_uniforms;   // uniforms déclarés : on n'envoie que ceux-là
    sf::Clock m_clock;
};
