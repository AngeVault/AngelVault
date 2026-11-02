#pragma once
// Registre « nom de composant -> écrire / lire en JSON ».
//
// Chaque composant sauvegardable a un ComponentSerializer. Ceux du moteur sont déjà enregistrés
// (Name, Transform2D, Geometry, RigidBody2D, Collider2D, Camera2d, Script). Pour un nouveau composant :
//
//     SerializerRegistry::get().add<MonComposant>("MonComposant",
//         [](MonComposant& c, const SaveContext&) { return nlohmann::json{ { "vie", c.vie } }; },
//         [](MonComposant& c, const nlohmann::json& j, const LoadContext&) { c.vie = j.value("vie", c.vie); });
//
// ou, s'il a un reflectComponent<T>() : SerializerRegistry::get().addReflected<MonComposant>("MonComposant");
//
// Règle importante : un champ de type Entity s'écrit avec ctx.idOf(e) et se relit avec ctx.entityOf(id)
// (voir PersistentId).
#include "ecs/entity.h"
#include "ecs/reflect.h"
#include "ecs/registry.h"

#include <nlohmann/json.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

using Json = nlohmann::ordered_json;   // garde l'ordre des clés : fichiers plus lisibles

// Sauvegarde : Entity -> PersistentId (0 = aucune entité)
struct SaveContext {
    std::function<uint64_t(Entity)> idOf;
};

// Chargement : PersistentId -> Entity de cette session (NULL_ENTITY si introuvable)
struct LoadContext {
    std::function<Entity(uint64_t)> entityOf;
    std::function<void(const std::string&)> warn;   // message pour la console (peut être vide)
};

struct ComponentSerializer {
    std::string name;   // clé dans le fichier : "Transform2D"
    std::function<bool(Registry&, Entity)> has;
    std::function<Json(Registry&, Entity, const SaveContext&)> save;
    std::function<void(Registry&, Entity, const Json&, const LoadContext&)> load;   // ajoute le composant
};

class SerializerRegistry {
public:
    static SerializerRegistry& get();   // déjà rempli avec les composants du moteur

    // save(T&, ctx) -> Json ; load(T&, json, ctx). Le composant est créé avec T{} avant load.
    template<typename T, typename SaveFn, typename LoadFn>
    void add(const std::string& name, SaveFn save, LoadFn load) {
        ComponentSerializer s;
        s.name = name;
        s.has = [](Registry& r, Entity e) { return r.hasComponent<T>(e); };
        s.save = [save](Registry& r, Entity e, const SaveContext& ctx) { return Json(save(r.getComponent<T>(e), ctx)); };
        s.load = [load](Registry& r, Entity e, const Json& j, const LoadContext& ctx) {
            T& c = r.hasComponent<T>(e) ? r.getComponent<T>(e) : r.addComponent<T>(e);
            load(c, j, ctx);
        };
        addSerializer(std::move(s));
    }

    // Composant décrit par reflectComponent<T>() : tous ses champs réfléchis sont sauvegardés
    template<typename T>
    void addReflected(const std::string& name);

    void addSerializer(ComponentSerializer serializer);   // remplace un serializer du même nom
    const ComponentSerializer* find(const std::string& name) const;
    const std::vector<ComponentSerializer>& all() const { return m_serializers; }

private:
    SerializerRegistry() = default;
    void syncGameComponents();   // ajoute les composants de jeu (REGISTER_COMPONENT) pas encore connus
    std::vector<ComponentSerializer> m_serializers;
    std::size_t m_gameComponents = 0;
};

// ---- Aides JSON (aussi utiles pour vos propres serializers) --------------------------------------
namespace SceneJson {

inline Json vec2(sf::Vector2f v) { return Json::array({ v.x, v.y }); }
inline sf::Vector2f vec2(const Json& j, sf::Vector2f fallback = {}) {
    if (!j.is_array() || j.size() < 2) return fallback;
    return { j[0].get<float>(), j[1].get<float>() };
}

inline Json rect(const sf::IntRect& r) { return Json::array({ r.position.x, r.position.y, r.size.x, r.size.y }); }
inline sf::IntRect rect(const Json& j, sf::IntRect fallback = {}) {
    if (!j.is_array() || j.size() < 4) return fallback;
    return sf::IntRect({ j[0].get<int>(), j[1].get<int>() }, { j[2].get<int>(), j[3].get<int>() });
}

Json color(sf::Color c);                                  // "#RRGGBBAA"
sf::Color color(const Json& j, sf::Color fallback = sf::Color::White);

Json property(const PropertyInfo& p);                     // valeur d'un champ réfléchi / PROPERTY
void readProperty(const Json& j, PropertyInfo& p);        // ignore une valeur du mauvais type

template<typename T>
T value(const Json& j, const char* key, T fallback) {     // comme j.value(), sans exception sur un mauvais type
    const auto it = j.find(key);
    if (it == j.end()) return fallback;
    try { return it->template get<T>(); } catch (...) { return fallback; }
}

} // namespace SceneJson

template<typename T>
void SerializerRegistry::addReflected(const std::string& name) {
    add<T>(name,
        [](T& c, const SaveContext&) {
            Json j = Json::object();
            for (const PropertyInfo& p : reflectComponent<T>(c).fields) j[p.name] = SceneJson::property(p);
            return j;
        },
        [](T& c, const Json& j, const LoadContext&) {
            for (PropertyInfo& p : reflectComponent<T>(c).fields)
                if (j.contains(p.name)) SceneJson::readProperty(j[p.name], p);
        });
}
