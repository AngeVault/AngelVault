#include "scene/component_serializer.h"

#include "components/camera2d.h"
#include "components/collider2d.h"
#include "components/geometry.h"
#include "components/name.h"
#include "components/rigid_body2d.h"
#include "components/script_component.h"
#include "components/transform2d.h"
#include "tilemap/tilemap.h"
#include "ecs/behavior_factory.h"
#include "ecs/game_components.h"

#include <cstdio>
#include <cstdlib>

using namespace SceneJson;

// ---- Aides JSON --------------------------------------------------------------------------------

Json SceneJson::color(sf::Color c) {
    char buf[10];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
    return buf;
}

sf::Color SceneJson::color(const Json& j, sf::Color fallback) {
    if (!j.is_string()) return fallback;
    const std::string s = j.get<std::string>();
    if (s.size() != 7 && s.size() != 9) return fallback;
    if (s[0] != '#') return fallback;
    const unsigned long v = std::strtoul(s.c_str() + 1, nullptr, 16);
    if (s.size() == 7) return sf::Color(uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v), 255);
    return sf::Color(uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v));
}

Json SceneJson::property(const PropertyInfo& p) {
    switch (p.type) {
    case PropertyInfo::Type::Float:  return p.as<float>();
    case PropertyInfo::Type::Int:    return p.as<int>();
    case PropertyInfo::Type::Bool:   return p.as<bool>();
    case PropertyInfo::Type::Vec2f:  return vec2(p.as<sf::Vector2f>());
    case PropertyInfo::Type::String: return p.as<std::string>();
    }
    return nullptr;
}

void SceneJson::readProperty(const Json& j, PropertyInfo& p) {
    switch (p.type) {
    case PropertyInfo::Type::Float:  if (j.is_number()) p.as<float>() = j.get<float>(); break;
    case PropertyInfo::Type::Int:    if (j.is_number()) p.as<int>() = j.get<int>(); break;
    case PropertyInfo::Type::Bool:   if (j.is_boolean()) p.as<bool>() = j.get<bool>(); break;
    case PropertyInfo::Type::Vec2f:  if (j.is_array()) p.as<sf::Vector2f>() = vec2(j, p.as<sf::Vector2f>()); break;
    case PropertyInfo::Type::String: if (j.is_string()) p.as<std::string>() = j.get<std::string>(); break;
    }
}

// ---- Registre ------------------------------------------------------------------------------------

void SerializerRegistry::addSerializer(ComponentSerializer serializer) {
    for (ComponentSerializer& s : m_serializers)
        if (s.name == serializer.name) { s = std::move(serializer); return; }
    m_serializers.push_back(std::move(serializer));
}

const ComponentSerializer* SerializerRegistry::find(const std::string& name) const {
    for (const ComponentSerializer& s : m_serializers)
        if (s.name == name) return &s;
    return nullptr;
}

namespace {

// Formes de Geometry : même ordre que Geometry::GeoData
const char* const kGeometryShapes[] = { "Box", "Circle", "Polygon", "Sprite" };

Json styleToJson(const Geometry::ShapeStyle& s) {
    Json j = { { "fill", color(s.fillColor) }, { "outline", color(s.outlineColor) },
               { "outlineThickness", s.outlineThickness } };
    if (!s.textureId.empty()) j["texture"] = s.textureId;
    return j;
}

Geometry::ShapeStyle styleFromJson(const Json& j) {
    Geometry::ShapeStyle s;
    s.fillColor = color(j.value("fill", Json()), s.fillColor);
    s.outlineColor = color(j.value("outline", Json()), s.outlineColor);
    s.outlineThickness = value(j, "outlineThickness", s.outlineThickness);
    s.textureId = value<std::string>(j, "texture", "");
    return s;
}

Json pointsToJson(const std::vector<sf::Vector2f>& points) {
    Json a = Json::array();
    for (sf::Vector2f p : points) a.push_back(vec2(p));
    return a;
}

std::vector<sf::Vector2f> pointsFromJson(const Json& j) {
    std::vector<sf::Vector2f> points;
    if (j.is_array()) for (const Json& p : j) points.push_back(vec2(p));
    return points;
}

void registerBuiltins(SerializerRegistry& reg) {
    // ---- Name
    reg.add<NameComponent>("Name",
        [](NameComponent& c, const SaveContext&) { return Json{ { "name", c.name } }; },
        [](NameComponent& c, const Json& j, const LoadContext&) { c.name = value<std::string>(j, "name", c.name); });

    // ---- Transform2D (position / rotation / échelle privées : on passe par les accesseurs)
    reg.add<Transform2D>("Transform2D",
        [](Transform2D& t, const SaveContext&) {
            return Json{ { "position", vec2(t.getPosition()) }, { "rotation", t.getRotation().asDegrees() },
                         { "scale", vec2(t.getScale()) }, { "depth", t.depth } };
        },
        [](Transform2D& t, const Json& j, const LoadContext&) {
            t.setPosition(vec2(j.value("position", Json()), t.getPosition()));
            t.setRotation(sf::degrees(value(j, "rotation", t.getRotation().asDegrees())));
            t.setScale(vec2(j.value("scale", Json()), t.getScale()));
            t.depth = value(j, "depth", t.depth);
        });

    // ---- Geometry (std::variant : "shape" dit laquelle)
    reg.add<Geometry>("Geometry",
        [](Geometry& g, const SaveContext&) {
            Json j = { { "layer", g.layer }, { "shape", kGeometryShapes[g.data.index()] } };
            std::visit([&](auto& d) {
                using T = std::decay_t<decltype(d)>;
                if constexpr (std::is_same_v<T, Geometry::BoxData>) { j["halfSize"] = vec2(d.halfSize); j["style"] = styleToJson(d.style); }
                else if constexpr (std::is_same_v<T, Geometry::CircleData>) { j["radius"] = d.radius; j["style"] = styleToJson(d.style); }
                else if constexpr (std::is_same_v<T, Geometry::PolygonData>) { j["points"] = pointsToJson(d.points); j["style"] = styleToJson(d.style); }
                else { j["texture"] = d.textureId; j["rect"] = rect(d.rect); j["tint"] = color(d.tint); }
            }, g.data);
            return j;
        },
        [](Geometry& g, const Json& j, const LoadContext& ctx) {
            g.layer = value(j, "layer", g.layer);
            const std::string shape = value<std::string>(j, "shape", "Box");
            const Json style = j.value("style", Json::object());
            if (shape == "Circle") g.data = Geometry::CircleData{ value(j, "radius", 16.f), styleFromJson(style) };
            else if (shape == "Polygon") g.data = Geometry::PolygonData{ pointsFromJson(j.value("points", Json())), styleFromJson(style) };
            else if (shape == "Sprite")
                g.data = Geometry::SpriteData{ value<std::string>(j, "texture", ""), rect(j.value("rect", Json())),
                                               color(j.value("tint", Json()), sf::Color::White) };
            else {
                if (shape != "Box" && ctx.warn) ctx.warn("Geometry : forme inconnue « " + shape + " », remplacée par Box");
                g.data = Geometry::BoxData{ vec2(j.value("halfSize", Json()), { 16.f, 16.f }), styleFromJson(style) };
            }
        });

    // ---- RigidBody2D : champs réfléchis (mass, linearDrag, friction, isStatic, useGravity, inertia, fixedRotation)
    reg.addReflected<RigidBody2D>("RigidBody2D");

    // ---- Collider2D
    reg.add<Collider2D>("Collider2D",
        [](Collider2D& c, const SaveContext&) {
            Json j = { { "offset", vec2(c.offset) }, { "isTrigger", c.isTrigger },
                       { "restitution", c.restitution }, { "friction", c.friction } };
            std::visit([&](auto& s) {
                using T = std::decay_t<decltype(s)>;
                if constexpr (std::is_same_v<T, Collider2D::Box>) { j["shape"] = "Box"; j["halfSize"] = vec2(s.halfSize); }
                else if constexpr (std::is_same_v<T, Collider2D::Circle>) { j["shape"] = "Circle"; j["radius"] = s.radius; }
                else if constexpr (std::is_same_v<T, Collider2D::Ellipse>) { j["shape"] = "Ellipse"; j["radii"] = vec2(s.radii); }
                else if constexpr (std::is_same_v<T, Collider2D::Capsule>) { j["shape"] = "Capsule"; j["radius"] = s.radius; j["height"] = s.height; }
                else { j["shape"] = "Polygon"; j["points"] = pointsToJson(s.points); }
            }, c.shape);
            return j;
        },
        [](Collider2D& c, const Json& j, const LoadContext&) {
            c.offset = vec2(j.value("offset", Json()), c.offset);
            c.isTrigger = value(j, "isTrigger", c.isTrigger);
            c.restitution = value(j, "restitution", c.restitution);
            c.friction = value(j, "friction", c.friction);
            const std::string shape = value<std::string>(j, "shape", "Box");
            if (shape == "Circle") c.shape = Collider2D::Circle{ value(j, "radius", 16.f) };
            else if (shape == "Ellipse") c.shape = Collider2D::Ellipse{ vec2(j.value("radii", Json()), { 16.f, 8.f }) };
            else if (shape == "Capsule") c.shape = Collider2D::Capsule{ value(j, "radius", 8.f), value(j, "height", 24.f) };
            else if (shape == "Polygon") c.shape = Collider2D::Polygon{ pointsFromJson(j.value("points", Json())) };
            else c.shape = Collider2D::Box{ vec2(j.value("halfSize", Json()), { 16.f, 16.f }) };
        });

    // ---- Camera2d (targetLock est une Entity : passe par les PersistentId)
    reg.add<Camera2d>("Camera2d",
        [](Camera2d& c, const SaveContext& ctx) {
            Json j = Json::object();
            for (const PropertyInfo& p : reflectComponent<Camera2d>(c).fields) j[p.name] = property(p);
            j["worldMin"] = vec2(c.worldMin);
            j["worldMax"] = vec2(c.worldMax);
            const uint64_t target = ctx.idOf ? ctx.idOf(c.targetLock) : 0;
            j["targetLock"] = target ? Json(target) : Json(nullptr);
            return j;
        },
        [](Camera2d& c, const Json& j, const LoadContext& ctx) {
            for (PropertyInfo& p : reflectComponent<Camera2d>(c).fields)
                if (j.contains(p.name)) readProperty(j[p.name], p);
            c.worldMin = vec2(j.value("worldMin", Json()), c.worldMin);
            c.worldMax = vec2(j.value("worldMax", Json()), c.worldMax);
            c.targetLock = NULL_ENTITY;
            const auto it = j.find("targetLock");
            if (it != j.end() && it->is_number_unsigned()) {
                const uint64_t id = it->get<uint64_t>();
                c.targetLock = ctx.entityOf ? ctx.entityOf(id) : NULL_ENTITY;
                if (c.targetLock == NULL_ENTITY && ctx.warn)
                    ctx.warn("Camera2d : l'entité suivie (" + std::to_string(id) + ") n'existe plus, la caméra devient libre");
            }
        });

    // ---- Tilemap : seulement le fichier .map (les cases sont dedans) et « visible »
    reg.add<Tilemap>("Tilemap",
        [](Tilemap& t, const SaveContext&) { return Json{ { "file", t.file }, { "visible", t.visible } }; },
        [](Tilemap& t, const Json& j, const LoadContext&) {
            t.file = value<std::string>(j, "file", t.file);
            t.visible = value(j, "visible", t.visible);
            t.data.reset();   // rechargée par loadTilemaps()
        });

    // ---- Script : liste de comportements, chacun = son nom (BehaviorFactory) + ses PROPERTY
    //   "Script": { "behaviors": [ { "name": "PlayerBehavior", "properties": { "speed": 250 } } ] }
    reg.add<Script>("Script",
        [](Script& s, const SaveContext&) {
            Json list = Json::array();
            for (const Script::Slot& slot : s.behaviors) {
                if (!slot.behavior) continue;
                Json props = Json::object();
                for (const PropertyInfo& p : slot.behavior->m_properties) props[p.name] = property(p);
                list.push_back({ { "name", BehaviorFactory::get().nameOf(*slot.behavior) }, { "properties", props } });
            }
            return Json{ { "behaviors", list } };
        },
        [](Script& s, const Json& j, const LoadContext& ctx) {
            s.behaviors.clear();
            Json list = j.value("behaviors", Json::array());
            if (j.contains("behavior") && j["behavior"].is_string())   // ancien format : un seul comportement
                list = Json::array({ { { "name", j["behavior"] }, { "properties", j.value("properties", Json::object()) } } });
            if (!list.is_array()) return;
            for (const Json& entry : list) {
                const std::string name = value<std::string>(entry, "name", "");
                if (name.empty()) {
                    if (ctx.warn) ctx.warn("Script : comportement sans nom (ajoutez REGISTER_BEHAVIOR sous sa classe)");
                    continue;
                }
                std::unique_ptr<IBehavior> b = BehaviorFactory::get().create(name);
                if (!b) {
                    if (ctx.warn) ctx.warn("Script : comportement inconnu « " + name + " » (fichier supprimé, ou pas encore compilé ?)");
                    continue;
                }
                const Json props = entry.value("properties", Json::object());
                for (PropertyInfo& p : b->m_properties)
                    if (props.contains(p.name)) readProperty(props[p.name], p);
                s.add(std::move(b));
            }
        });
}

} // namespace

SerializerRegistry& SerializerRegistry::get() {
    static SerializerRegistry instance = [] {
        SerializerRegistry r;
        registerBuiltins(r);
        return r;
    }();
    instance.syncGameComponents();
    return instance;
}

// Composants de jeu (projet) : enregistrés champ par champ grâce à REGISTER_COMPONENT / FIELD
void SerializerRegistry::syncGameComponents() {
    const auto& game = GameComponentRegistry::get().all();
    if (game.size() == m_gameComponents) return;
    for (std::size_t i = m_gameComponents; i < game.size(); ++i) {
        const GameComponentInfo info = game[i];
        ComponentSerializer s;
        s.name = info.name;
        s.has = info.has;
        s.save = [info](Registry& r, Entity e, const SaveContext&) {
            Json j = Json::object();
            for (const PropertyInfo& p : info.reflect(r, e).fields) j[p.name] = property(p);
            return j;
        };
        s.load = [info](Registry& r, Entity e, const Json& j, const LoadContext&) {
            info.add(r, e);
            for (PropertyInfo& p : info.reflect(r, e).fields)
                if (j.contains(p.name)) readProperty(j[p.name], p);
        };
        addSerializer(std::move(s));
    }
    m_gameComponents = game.size();
}
