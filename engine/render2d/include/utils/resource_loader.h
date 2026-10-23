#pragma once
#include <SFML/Graphics.hpp>
#include <string>

struct IResourceLoader {
    virtual ~IResourceLoader() = default;
    virtual sf::Texture& loadTexture(const std::string& id) = 0;
    virtual void unloadTexture(const std::string& id) = 0;
    // virtual void loadTextureAsync(...) — plus tard, si le besoin 
};