#pragma once
#include "resource_loader.h"
#include "texture_factory.h"
#include <SFML/Graphics.hpp>
#include <unordered_map>
#include <string>
#include <fstream>
#include <iostream>

class LooseFileLoader : public IResourceLoader {
public:
	explicit LooseFileLoader(const std::string& mappingFile = "games/rts/assets/resources.txt") {
		std::cout << "loading path from : " << mappingFile << std::endl;
		std::ifstream file(mappingFile);
		std::string id, path;
		while (file >> id >> path)
			m_idToPath[id] = path;
	}

	sf::Texture& loadTexture(const std::string& id) override 
	{
		
		if (auto it = m_cache.find(id); it != m_cache.end())
			return it->second;

		std::string path = id;
		if (auto it = m_idToPath.find(id); it != m_idToPath.end())
			path = it->second;

		sf::Texture tex; 
		if (!tex.loadFromFile( path))
		{
			std::cout << "couldn't find " << path << std::endl;
			tex = TextureFactory::createCheckerboard({ 32, 32 }, sf::Color(255, 0, 255), sf::Color::Black);
		}
		auto [inserted, _] = m_cache.emplace(id, std::move(tex));
		return inserted->second;
	}

	void unloadTexture(const std::string& id) override {
		m_cache.erase(id);
	}

private:
	std::unordered_map<std::string, std::string> m_idToPath;
	std::unordered_map<std::string, sf::Texture> m_cache;
};
