#pragma once
// Fabrique de comportements (scripts) : nom -> nouvelle instance.
//
// Un IBehavior est du code C++ : on ne peut pas l'écrire dans un fichier. La scène enregistre
// donc son NOM et ses PROPERTY ; au chargement, la fabrique recrée le bon comportement.
//
// Une ligne par comportement, sous sa classe :
//     class PlayerBehavior : public IBehavior { ... };
//     REGISTER_BEHAVIOR(PlayerBehavior)
#include "ecs/behavior.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

class BehaviorFactory {
public:
    using Creator = std::function<std::unique_ptr<IBehavior>()>;

    static BehaviorFactory& get() {
        static BehaviorFactory instance;
        return instance;
    }

    template<typename T>
    bool add(const std::string& name, const std::string& sourceFile = {}) {
        m_creators[name] = [] { return std::make_unique<T>(); };
        m_names[std::type_index(typeid(T))] = name;
        m_files[name] = sourceFile;
        return true;
    }

    // nullptr si le nom est inconnu (comportement renommé ou supprimé du code)
    std::unique_ptr<IBehavior> create(const std::string& name) const {
        const auto it = m_creators.find(name);
        return it != m_creators.end() ? it->second() : nullptr;
    }

    // Nom d'une instance existante ("" si sa classe n'est pas enregistrée)
    std::string nameOf(const IBehavior& behavior) const {
        const auto it = m_names.find(std::type_index(typeid(behavior)));
        return it != m_names.end() ? it->second : std::string();
    }

    // Fichier source où le comportement est déclaré (__FILE__ de REGISTER_BEHAVIOR)
    std::string fileOf(const std::string& name) const {
        const auto it = m_files.find(name);
        return it != m_files.end() ? it->second : std::string();
    }

    // Comportements déclarés dans un fichier, comparé par son NOM (ex. "player_behaviors.h") :
    // c'est ce que l'éditeur utilise quand on glisse un fichier sur une entité.
    std::vector<std::string> namesInFile(const std::string& fileName) const {
        std::vector<std::string> out;
        for (const auto& [name, file] : m_files)
            if (baseName(file) == baseName(fileName)) out.push_back(name);
        std::sort(out.begin(), out.end());
        return out;
    }

    std::vector<std::string> names() const {   // triés, pour un menu de l'éditeur
        std::vector<std::string> out;
        for (const auto& [name, _] : m_creators) out.push_back(name);
        std::sort(out.begin(), out.end());
        return out;
    }

private:
    static std::string baseName(const std::string& path) {
        const auto slash = path.find_last_of("/\\");
        std::string b = slash == std::string::npos ? path : path.substr(slash + 1);
        for (char& ch : b) ch = char(std::tolower(static_cast<unsigned char>(ch)));   // Windows : casse ignorée
        return b;
    }

    std::unordered_map<std::string, Creator> m_creators;
    std::unordered_map<std::string, std::string> m_files;
    std::unordered_map<std::type_index, std::string> m_names;
};

// `inline` : la ligne peut être dans un .h inclus plusieurs fois, l'enregistrement n'a lieu qu'une fois
#define REGISTER_BEHAVIOR(T) \
    inline const bool g_avBehaviorRegistered_##T = BehaviorFactory::get().add<T>(#T, __FILE__);
