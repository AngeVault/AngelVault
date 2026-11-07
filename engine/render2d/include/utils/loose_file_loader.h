#pragma once
#include "resource_loader.h"
#include "texture_factory.h"
#include <SFML/Graphics.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>

// Textures chargées depuis des fichiers. Un id est un chemin : relatif au projet (setBaseDir) ou absolu.
class LooseFileLoader : public IResourceLoader {
public:
	explicit LooseFileLoader(std::string baseDir = "") : m_baseDir(std::move(baseDir)) {}

	void setBaseDir(std::string dir) { m_baseDir = std::move(dir); }
	const std::string& baseDir() const { return m_baseDir; }

	sf::Texture& loadTexture(const std::string& id) override
	{
		const std::filesystem::path path = resolve(id);
		const std::string key = keyOf(path);
		if (auto it = m_cache.find(key); it != m_cache.end())
			return it->second;

		sf::Texture tex;
		if (!tex.loadFromFile(path))
		{
			std::cout << "couldn't find " << key << std::endl;
			tex = TextureFactory::createCheckerboard({ 32, 32 }, sf::Color(255, 0, 255), sf::Color::Black);
		}
		return m_cache.emplace(key, std::move(tex)).first->second;
	}

	void unloadTexture(const std::string& id) override {
		m_cache.erase(keyOf(resolve(id)));
	}

private:
	// UTF-8 -> path (dossiers avec accents sous Windows)
	static std::filesystem::path u8path(const std::string& s) { return std::u8string(s.begin(), s.end()); }
	static std::string keyOf(const std::filesystem::path& p) {
		const std::u8string s = p.generic_u8string();
		return std::string(s.begin(), s.end());
	}

	std::filesystem::path resolve(const std::string& id) const {
		const std::filesystem::path p = u8path(id);
		if (p.is_absolute() || m_baseDir.empty()) return p;
		return (u8path(m_baseDir) / p).lexically_normal();
	}

	std::string m_baseDir;
	std::unordered_map<std::string, sf::Texture> m_cache;   // clé : chemin résolu
};
