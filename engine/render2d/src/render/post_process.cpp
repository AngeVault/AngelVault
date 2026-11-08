#include "render/post_process.h"

#include <SFML/Graphics/Sprite.hpp>

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace {
// uniform <type> <nom>;
const std::regex kUniform(R"(uniform\s+\w+\s+(\w+)\s*;)");
// uniform float <nom>; // @param <défaut> <min> <max>
const std::regex kParam(R"(uniform\s+float\s+(\w+)\s*;\s*//\s*@param\s+(\S+)\s+(\S+)\s+(\S+))");

bool readText(const std::string& utf8Path, std::string& out) {
    std::ifstream in(std::filesystem::path(std::u8string(utf8Path.begin(), utf8Path.end())), std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}
} // namespace

std::vector<PostProcess::Param> PostProcess::parseParams(const std::string& source) {
    std::vector<Param> params;
    std::istringstream in(source);
    std::string line;
    std::smatch m;
    while (std::getline(in, line)) {
        if (!std::regex_search(line, m, kParam)) continue;
        try {
            Param p{ m[1].str(), std::stof(m[2].str()), std::stof(m[3].str()), std::stof(m[4].str()) };
            if (p.max > p.min) params.push_back(p);
        } catch (...) {}   // nombre illisible : réglage ignoré
    }
    return params;
}

bool PostProcess::load(const std::string& fragPath, std::string* error) {
    clear();
    if (fragPath.empty()) return true;
    std::string source;
    if (!readText(fragPath, source)) {
        if (error) *error = "shader introuvable : " + fragPath;
        return false;
    }
    if (!sf::Shader::isAvailable()) {
        if (error) *error = "shaders non supportés par la carte graphique";
        return false;
    }
    sf::Shader shader;
    if (!shader.loadFromMemory(source, sf::Shader::Type::Fragment)) {
        if (error) *error = "shader invalide : " + fragPath;
        return false;
    }
    m_shader.emplace(std::move(shader));
    m_params = parseParams(source);
    for (auto it = std::sregex_iterator(source.begin(), source.end(), kUniform); it != std::sregex_iterator(); ++it)
        m_uniforms.insert((*it)[1].str());
    m_clock.restart();
    return true;
}

void PostProcess::clear() {
    m_shader.reset();
    m_params.clear();
    m_uniforms.clear();
}

void PostProcess::setParams(const std::map<std::string, float>& values) {
    for (Param& p : m_params)
        if (auto it = values.find(p.name); it != values.end()) p.value = it->second;
}

void PostProcess::apply(sf::RenderWindow& window) {
    if (!m_shader) return;
    const sf::Vector2u size = window.getSize();
    if (m_frame.getSize() != size && !m_frame.resize(size)) return;
    m_frame.update(window);

    sf::Shader& shader = *m_shader;
    if (m_uniforms.count("frame")) shader.setUniform("frame", sf::Shader::CurrentTexture);
    if (m_uniforms.count("resolution")) shader.setUniform("resolution", sf::Vector2f(size));
    if (m_uniforms.count("time")) shader.setUniform("time", m_clock.getElapsedTime().asSeconds());
    for (const Param& p : m_params) shader.setUniform(p.name, p.value);

    // l'image entière, quelle que soit la caméra du jeu
    // pas getDefaultView() : elle garde la taille de création de la fenêtre (widget redimensionné dans l'éditeur)
    const sf::View view = window.getView();
    window.setView(sf::View(sf::FloatRect({ 0.f, 0.f }, sf::Vector2f(size))));
    window.draw(sf::Sprite(m_frame), &shader);
    window.setView(view);
}
