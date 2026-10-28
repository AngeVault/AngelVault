#include "component_descriptors.h"

#include "components/camera2d.h"
#include "components/collider2d.h"
#include "components/geometry.h"
#include "components/name.h"
#include "components/rigid_body2d.h"
#include "components/script_component.h"
#include "components/transform2d.h"

#include <QColor>
#include <QPointF>
#include <cmath>
#include <typeinfo>

/**
 * @file component_descriptors.cpp
 * @brief Builds metadata used by the editor to inspect and edit ECS components.
 *
 * This file is the bridge between the runtime ECS component types and the Qt-based
 * inspector UI. Its purpose is to describe, for each component:
 * - whether it exists on an entity,
 * - how to add/remove it,
 * - which fields to display and how to read/write them.
 *
 * The model is intentionally generic: helper functions generate FieldDesc values
 * from component member references, so the editor can expose values to widgets
 * without needing hardcoded per-field code for each component.
 *
 * The descriptor system is centered around:
 * - QVariant conversion between SFML and Qt types,
 * - per-component field builders,
 * - runtime reflection for script-driven behavior properties.
 */

namespace {

/**
 * Converts an SFML 2D vector into a Qt point-based QVariant.
 *
 * The editor UI generally uses QPointF/QVariant to represent 2D values.
 *
 * @param v The SFML vector to convert.
 * @return A QVariant carrying a QPointF.
 */
QVariant v2(sf::Vector2f v) { return QPointF(v.x, v.y); }

/**
 * Converts a QVariant representing a 2D point into an SFML vector.
 *
 * @param v The QVariant carrying a 2D point.
 * @return The equivalent sf::Vector2f.
 */
sf::Vector2f toV2(const QVariant& v) { const QPointF p = v.toPointF(); return { float(p.x()), float(p.y()) }; }

/**
 * Converts an SFML integer vector into a Qt point-based QVariant.
 *
 * @param v The integer SFML vector.
 * @return A QVariant containing the point representation.
 */
QVariant v2i(sf::Vector2i v) { return QPointF(v.x, v.y); }

/**
 * Converts a QVariant point into an SFML integer vector.
 *
 * @param v QVariant containing point coordinates.
 * @return Equivalent sf::Vector2i.
 */
sf::Vector2i toV2i(const QVariant& v) { const QPointF p = v.toPointF(); return { int(p.x()), int(p.y()) }; }

/**
 * Converts an SFML color into a QVariant backed by QColor.
 *
 * @param c SFML color.
 * @return QVariant containing a QColor.
 */
QVariant col(sf::Color c) { return QColor(c.r, c.g, c.b, c.a); }

/**
 * Converts a QColor QVariant into an SFML color.
 *
 * @param v QVariant representing a QColor.
 * @return Equivalent sf::Color with alpha preserved.
 */
sf::Color toCol(const QVariant& v) {
    const QColor c = v.value<QColor>();
    return sf::Color(uint8_t(c.red()), uint8_t(c.green()), uint8_t(c.blue()), uint8_t(c.alpha()));
}

/**
 * Creates a FieldDesc for a floating-point property backed by a component member.
 *
 * This helper is used to expose a field such as mass, radius, friction, etc.
 * as editable data in the inspector. The getter/setter capture the target
 * entity and component by reference and read/write the underlying value.
 *
 * @tparam C Component type.
 * @tparam Acc Accessor function returning a float reference for the component.
 * @param r Registry used to access the component.
 * @param e Entity owning the component.
 * @param name User-facing field name.
 * @param acc Function returning a reference to the underlying float.
 * @param min Minimum value allowed in the inspector.
 * @param max Maximum value allowed in the inspector.
 * @param step Increment used by numeric editors.
 * @return A field description ready to be displayed in the editor.
 */
template<typename C, typename Acc>
FieldDesc floatRef(Registry& r, Entity e, QString name, Acc acc, double min = -1e9, double max = 1e9, double step = 1.0) {
    FieldDesc f{ name, FieldKind::Float };
    f.get = [&r, e, acc] { return QVariant(double(acc(r.getComponent<C>(e)))); };
    f.set = [&r, e, acc](const QVariant& v) { acc(r.getComponent<C>(e)) = float(v.toDouble()); };
    f.min = min; f.max = max; f.step = step;
    return f;
}

/**
 * Creates a FieldDesc for an integer property backed by a component member.
 *
 * @tparam C Component type.
 * @tparam Acc Accessor function returning an int reference.
 * @param r Registry used to access the component.
 * @param e Entity owning the component.
 * @param name User-facing field name.
 * @param acc Function returning a reference to the integer field.
 * @return A field description ready for the inspector.
 */
template<typename C, typename Acc>
FieldDesc intRef(Registry& r, Entity e, QString name, Acc acc) {
    FieldDesc f{ name, FieldKind::Int };
    f.get = [&r, e, acc] { return QVariant(acc(r.getComponent<C>(e))); };
    f.set = [&r, e, acc](const QVariant& v) { acc(r.getComponent<C>(e)) = v.toInt(); };
    return f;
}

/**
 * Creates a FieldDesc for a boolean property backed by a component member.
 *
 * @tparam C Component type.
 * @tparam Acc Accessor function returning a bool reference.
 * @param r Registry used to access the component.
 * @param e Entity owning the component.
 * @param name User-facing field name.
 * @param acc Function returning a reference to the boolean field.
 * @param readOnly If true, the field is shown but not editable.
 * @return A field description ready for the inspector.
 */
template<typename C, typename Acc>
FieldDesc boolRef(Registry& r, Entity e, QString name, Acc acc, bool readOnly = false) {
    FieldDesc f{ name, FieldKind::Bool };
    f.get = [&r, e, acc] { return QVariant(bool(acc(r.getComponent<C>(e)))); };
    if (!readOnly) f.set = [&r, e, acc](const QVariant& v) { acc(r.getComponent<C>(e)) = v.toBool(); };
    return f;
}

/**
 * Creates a FieldDesc for a 2D vector property backed by a component member.
 *
 * @tparam C Component type.
 * @tparam Acc Accessor function returning an sf::Vector2f reference.
 * @param r Registry used to access the component.
 * @param e Entity owning the component.
 * @param name User-facing field name.
 * @param acc Function returning a reference to the vector field.
 * @param step Numeric increment used when editing the vector.
 * @return A field description ready for the inspector.
 */
template<typename C, typename Acc>
FieldDesc vecRef(Registry& r, Entity e, QString name, Acc acc, double step = 1.0) {
    FieldDesc f{ name, FieldKind::Vec2 };
    f.get = [&r, e, acc] { return v2(acc(r.getComponent<C>(e))); };
    f.set = [&r, e, acc](const QVariant& v) { acc(r.getComponent<C>(e)) = toV2(v); };
    f.step = step;
    return f;
}

/**
 * Creates a FieldDesc for a color property backed by a component member.
 *
 * @tparam C Component type.
 * @tparam Acc Accessor function returning an sf::Color reference.
 * @param r Registry used to access the component.
 * @param e Entity owning the component.
 * @param name User-facing field name.
 * @param acc Function returning a reference to the color field.
 * @return A field description ready for the inspector.
 */
template<typename C, typename Acc>
FieldDesc colorRef(Registry& r, Entity e, QString name, Acc acc) {
    FieldDesc f{ name, FieldKind::Color };
    f.get = [&r, e, acc] { return col(acc(r.getComponent<C>(e))); };
    f.set = [&r, e, acc](const QVariant& v) { acc(r.getComponent<C>(e)) = toCol(v); };
    return f;
}

/**
 * Creates a FieldDesc for a string property backed by a component member.
 *
 * @tparam C Component type.
 * @tparam Acc Accessor function returning a std::string reference.
 * @param r Registry used to access the component.
 * @param e Entity owning the component.
 * @param name User-facing field name.
 * @param acc Function returning a reference to the string field.
 * @return A field description ready for the inspector.
 */
template<typename C, typename Acc>
FieldDesc stringRef(Registry& r, Entity e, QString name, Acc acc) {
    FieldDesc f{ name, FieldKind::String };
    f.get = [&r, e, acc] { return QVariant(QString::fromStdString(acc(r.getComponent<C>(e)))); };
    f.set = [&r, e, acc](const QVariant& v) { acc(r.getComponent<C>(e)) = v.toString().toStdString(); };
    return f;
}

/**
 * Creates a static label field.
 *
 * A label is not editable; it is used to display read-only information like
 * "Rectangle", "Cercle", or the name of a script behavior.
 *
 * @param name Field name shown in the inspector.
 * @param text Value to display.
 * @return A read-only FieldDesc of type Label.
 */
FieldDesc label(QString name, QString text) {
    FieldDesc f{ name, FieldKind::Label };
    f.get = [text] { return QVariant(text); };
    return f;
}

/**
 * Adds common drawing style fields to the output vector for geometry shapes.
 *
 * This groups properties shared by box, circle, and polygon shapes:
 * - fill color,
 * - outline color,
 * - outline thickness,
 * - texture id.
 *
 * @tparam Pick Accessor returning a Geometry::ShapeStyle reference.
 * @param out Output vector receiving field descriptions.
 * @param r Registry used to access the Geometry component.
 * @param e Entity owning the Geometry.
 * @param pick Function selecting the appropriate ShapeStyle.
 */
template<typename Pick>
void styleFields(std::vector<FieldDesc>& out, Registry& r, Entity e, Pick pick) {
    out.push_back(colorRef<Geometry>(r, e, QObject::tr("Remplissage"), [pick](Geometry& g) -> sf::Color& { return pick(g).fillColor; }));
    out.push_back(colorRef<Geometry>(r, e, QObject::tr("Contour"), [pick](Geometry& g) -> sf::Color& { return pick(g).outlineColor; }));
    out.push_back(floatRef<Geometry>(r, e, QObject::tr("Épaisseur contour"),
                                     [pick](Geometry& g) -> float& { return pick(g).outlineThickness; }, 0, 100, 0.5));
    out.push_back(stringRef<Geometry>(r, e, QObject::tr("Texture"), [pick](Geometry& g) -> std::string& { return pick(g).textureId; }));
}

/**
 * Normalizes a C++ type name for display in the editor.
 *
 * This strips leading qualifiers such as "class " or "struct " and removes the
 * leading numeric prefix produced by some compilers for type names.
 *
 * Example:
 * - "class PlayerBehavior" -> "PlayerBehavior"
 * - "14PlayerBehavior" -> "PlayerBehavior"
 *
 * @param raw Raw type name from typeid(...).name().
 * @return The cleaned type name ready for UI display.
 */
QString cleanTypeName(const char* raw) {
    QString n = QString::fromLatin1(raw);
    n.remove(QStringLiteral("class ")).remove(QStringLiteral("struct "));
    int i = 0;
    while (i < n.size() && n[i].isDigit()) ++i;
    return n.mid(i);
}

/**
 * Changes the shape held by a Geometry (Rectangle / Cercle / Polygone / Sprite).
 *
 * The new shape keeps what makes sense from the old one: its size
 * (half-size, radius, sprite rect...) and its style (colors, texture).
 *
 * @param geo Geometry to modify.
 * @param kind Index of the new shape: 0 box, 1 circle, 2 polygon, 3 sprite (variant order).
 */
void setGeometryShape(Geometry& geo, int kind) {
    if (kind < 0 || kind > 3 || int(geo.data.index()) == kind) return;

    // Ce qu'on garde de l'ancienne forme : une demi-taille et un style
    sf::Vector2f half{ 16.f, 16.f };
    Geometry::ShapeStyle style;
    sf::IntRect spriteRect{};
    std::visit([&](auto& s) {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, Geometry::BoxData>) { half = s.halfSize; style = s.style; }
        else if constexpr (std::is_same_v<T, Geometry::CircleData>) { half = { s.radius, s.radius }; style = s.style; }
        else if constexpr (std::is_same_v<T, Geometry::PolygonData>) {
            style = s.style;
            if (!s.points.empty()) {
                sf::Vector2f m{ 0.f, 0.f };
                for (const sf::Vector2f& p : s.points) m = { std::max(m.x, std::abs(p.x)), std::max(m.y, std::abs(p.y)) };
                half = m;
            }
        } else {   // Sprite
            half = { s.rect.size.x / 2.f, s.rect.size.y / 2.f };
            style.fillColor = s.tint;
            style.textureId = s.textureId;
            spriteRect = s.rect;
        }
    }, geo.data);
    if (half.x <= 0.f) half.x = 16.f;
    if (half.y <= 0.f) half.y = 16.f;

    switch (kind) {
    case 0: geo.data = Geometry::BoxData{ half, style }; break;
    case 1: geo.data = Geometry::CircleData{ std::max(half.x, half.y), style }; break;
    case 2: {   // hexagone régulier centré (même repère que les autres formes)
        Geometry::PolygonData poly{ {}, style };
        for (int i = 0; i < 6; ++i) {
            const float a = 3.14159265f * 2.f * i / 6.f - 3.14159265f / 2.f;
            poly.points.push_back({ std::cos(a) * half.x, std::sin(a) * half.y });
        }
        geo.data = poly;
        break;
    }
    default: {
        if (spriteRect.size.x <= 0 || spriteRect.size.y <= 0)
            spriteRect = sf::IntRect({ 0, 0 }, { int(half.x * 2.f), int(half.y * 2.f) });
        geo.data = Geometry::SpriteData{ style.textureId, spriteRect, style.fillColor };
        break;
    }
    }
}

/**
 * Builds the full list of component descriptors for the editor inspector.
 *
 * Each descriptor contains:
 * - the component name,
 * - a predicate to test if the component exists on an entity,
 * - add/remove callbacks,
 * - field descriptions used by the property panel.
 *
 * @return A vector describing all supported components.
 */
std::vector<ComponentDesc> build() {
    std::vector<ComponentDesc> list;

    // ---------------------------------------------------------------- Transform2D
    {
        /**
         * Describes the Transform2D component.
         *
         * Exposes:
         * - position,
         * - rotation,
         * - scale,
         * - depth.
         */
        ComponentDesc d{ "Transform2D" };
        d.has = [](Registry& r, Entity e) { return r.hasComponent<Transform2D>(e); };
        d.add = [](Registry& r, Entity e) { r.addComponent<Transform2D>(e); };
        d.remove = [](Registry& r, Entity e) { r.removeComponent<Transform2D>(e); };
        d.fields = [](Registry& r, Entity e) {
            std::vector<FieldDesc> f;
            FieldDesc pos{ QObject::tr("Position"), FieldKind::Vec2 };
            pos.get = [&r, e] { return v2(r.getComponent<Transform2D>(e).getPosition()); };
            // Caméra qui suit une entité : position dictée par la cible -> lecture seule
            const bool locked = r.hasComponent<Camera2d>(e) && r.isValid(r.getComponent<Camera2d>(e).targetLock);
            if (!locked)
                pos.set = [&r, e](const QVariant& v) { r.getComponent<Transform2D>(e).setPosition(toV2(v)); };
            f.push_back(pos);

            FieldDesc rot{ QObject::tr("Rotation (°)"), FieldKind::Float };
            rot.get = [&r, e] { return QVariant(double(r.getComponent<Transform2D>(e).getRotation().asDegrees())); };
            rot.set = [&r, e](const QVariant& v) { r.getComponent<Transform2D>(e).setRotation(sf::degrees(float(v.toDouble()))); };
            rot.min = -36000; rot.max = 36000;
            f.push_back(rot);

            FieldDesc scale{ QObject::tr("Échelle"), FieldKind::Vec2 };
            scale.get = [&r, e] { return v2(r.getComponent<Transform2D>(e).getScale()); };
            scale.set = [&r, e](const QVariant& v) { r.getComponent<Transform2D>(e).setScale(toV2(v)); };
            scale.step = 0.1;
            f.push_back(scale);

            f.push_back(floatRef<Transform2D>(r, e, QObject::tr("Profondeur"), [](Transform2D& t) -> float& { return t.depth; }));
            return f;
        };
        list.push_back(d);
    }

    // ---------------------------------------------------------------- Geometry
    {
        /**
         * Describes the Geometry component.
         *
         * This component can hold several geometry variants:
         * - box,
         * - circle,
         * - polygon,
         * - sprite.
         *
         * Depending on the active variant, different fields are exposed:
         * - box: half-size and style settings,
         * - circle: radius and style settings,
         * - polygon: shape label and style settings,
         * - sprite: texture, rect position, rect size, tint.
         */
        ComponentDesc d{ "Geometry" };
        d.has = [](Registry& r, Entity e) { return r.hasComponent<Geometry>(e); };
        d.add = [](Registry& r, Entity e) {
            r.addComponent<Geometry>(e, Geometry::BoxData{ { 16.f, 16.f }, { sf::Color::White, sf::Color::Transparent, 0.f, "" } }, 1);
        };
        d.remove = [](Registry& r, Entity e) { r.removeComponent<Geometry>(e); };
        d.fields = [](Registry& r, Entity e) {
            std::vector<FieldDesc> f;
            f.push_back(intRef<Geometry>(r, e, QObject::tr("Calque"), [](Geometry& g) -> int& { return g.layer; }));
            Geometry& geo = r.getComponent<Geometry>(e);

            // Forme : liste déroulante (même ordre que le std::variant Geometry::GeoData)
            FieldDesc shape{ QObject::tr("Forme"), FieldKind::Choice };
            shape.choices = { QObject::tr("Rectangle"), QObject::tr("Cercle"), QObject::tr("Polygone"), QObject::tr("Sprite") };
            shape.get = [&r, e] { return QVariant(int(r.getComponent<Geometry>(e).data.index())); };
            shape.set = [&r, e](const QVariant& v) { setGeometryShape(r.getComponent<Geometry>(e), v.toInt()); };
            shape.rebuildOnChange = true;
            f.push_back(shape);

            if (std::holds_alternative<Geometry::BoxData>(geo.data)) {
                f.push_back(vecRef<Geometry>(r, e, QObject::tr("Demi-taille"),
                                             [](Geometry& g) -> sf::Vector2f& { return std::get<Geometry::BoxData>(g.data).halfSize; }));
                styleFields(f, r, e, [](Geometry& g) -> Geometry::ShapeStyle& { return std::get<Geometry::BoxData>(g.data).style; });
            } else if (std::holds_alternative<Geometry::CircleData>(geo.data)) {
                f.push_back(floatRef<Geometry>(r, e, QObject::tr("Rayon"),
                                               [](Geometry& g) -> float& { return std::get<Geometry::CircleData>(g.data).radius; }, 0));
                styleFields(f, r, e, [](Geometry& g) -> Geometry::ShapeStyle& { return std::get<Geometry::CircleData>(g.data).style; });
            } else if (std::holds_alternative<Geometry::PolygonData>(geo.data)) {
                const auto n = std::get<Geometry::PolygonData>(geo.data).points.size();
                f.push_back(label(QObject::tr("Points"), QString::number(n)));
                styleFields(f, r, e, [](Geometry& g) -> Geometry::ShapeStyle& { return std::get<Geometry::PolygonData>(g.data).style; });
            } else {
                f.push_back(stringRef<Geometry>(r, e, QObject::tr("Texture"),
                                                [](Geometry& g) -> std::string& { return std::get<Geometry::SpriteData>(g.data).textureId; }));
                FieldDesc pos{ QObject::tr("Zone : position"), FieldKind::Vec2 };
                pos.get = [&r, e] { return v2i(std::get<Geometry::SpriteData>(r.getComponent<Geometry>(e).data).rect.position); };
                pos.set = [&r, e](const QVariant& v) { std::get<Geometry::SpriteData>(r.getComponent<Geometry>(e).data).rect.position = toV2i(v); };
                pos.decimals = 0;
                f.push_back(pos);
                FieldDesc size{ QObject::tr("Zone : taille"), FieldKind::Vec2 };
                size.get = [&r, e] { return v2i(std::get<Geometry::SpriteData>(r.getComponent<Geometry>(e).data).rect.size); };
                size.set = [&r, e](const QVariant& v) { std::get<Geometry::SpriteData>(r.getComponent<Geometry>(e).data).rect.size = toV2i(v); };
                size.decimals = 0; size.min = 0;
                f.push_back(size);
                f.push_back(colorRef<Geometry>(r, e, QObject::tr("Teinte"),
                                               [](Geometry& g) -> sf::Color& { return std::get<Geometry::SpriteData>(g.data).tint; }));
            }
            return f;
        };
        list.push_back(d);
    }

    // ---------------------------------------------------------------- RigidBody2D
    {
        /**
         * Describes the RigidBody2D component.
         *
         * Exposes:
         * - mass,
         * - damping / drag,
         * - friction,
         * - static state,
         * - gravity usage,
         * - fixed rotation,
         * - velocity,
         * - grounded flag.
         */
        ComponentDesc d{ "RigidBody2D" };
        d.has = [](Registry& r, Entity e) { return r.hasComponent<RigidBody2D>(e); };
        d.add = [](Registry& r, Entity e) { r.addComponent<RigidBody2D>(e); };
        d.remove = [](Registry& r, Entity e) { r.removeComponent<RigidBody2D>(e); };
        d.fields = [](Registry& r, Entity e) {
            using RB = RigidBody2D;
            return std::vector<FieldDesc>{
                floatRef<RB>(r, e, QObject::tr("Masse"), [](RB& b) -> float& { return b.mass; }, 0.001, 1e9, 0.1),
                floatRef<RB>(r, e, QObject::tr("Amortissement"), [](RB& b) -> float& { return b.linearDrag; }, 0, 100, 0.01),
                floatRef<RB>(r, e, QObject::tr("Friction"), [](RB& b) -> float& { return b.friction; }, 0, 10, 0.05),
                boolRef<RB>(r, e, QObject::tr("Statique"), [](RB& b) -> bool& { return b.isStatic; }),
                boolRef<RB>(r, e, QObject::tr("Gravité"), [](RB& b) -> bool& { return b.useGravity; }),
                boolRef<RB>(r, e, QObject::tr("Rotation bloquée"), [](RB& b) -> bool& { return b.fixedRotation; }),
                vecRef<RB>(r, e, QObject::tr("Vitesse"), [](RB& b) -> sf::Vector2f& { return b.velocity; }),
                boolRef<RB>(r, e, QObject::tr("Au sol"), [](RB& b) -> bool& { return b.isGrounded; }, true),
            };
        };
        list.push_back(d);
    }

    // ---------------------------------------------------------------- Collider2D
    {
        /**
         * Describes the Collider2D component.
         *
         * The collider may use different shapes:
         * - Box,
         * - Circle,
         * - Ellipse,
         * - Capsule,
         * - Polygon.
         *
         * Each shape exposes specific geometry parameters, while common collider
         * properties are shown for all variants:
         * - offset,
         * - trigger flag,
         * - restitution,
         * - friction.
         */
        ComponentDesc d{ "Collider2D" };
        d.has = [](Registry& r, Entity e) { return r.hasComponent<Collider2D>(e); };
        d.add = [](Registry& r, Entity e) { r.addComponent<Collider2D>(e, Collider2D::makeBox({ 16.f, 16.f })); };
        d.remove = [](Registry& r, Entity e) { r.removeComponent<Collider2D>(e); };
        d.fields = [](Registry& r, Entity e) {
            using C = Collider2D;
            std::vector<FieldDesc> f;
            switch (r.getComponent<C>(e).type()) {
            case C::ShapeType::Box:
                f.push_back(label(QObject::tr("Forme"), QObject::tr("Rectangle")));
                f.push_back(vecRef<C>(r, e, QObject::tr("Demi-taille"), [](C& c) -> sf::Vector2f& { return std::get<C::Box>(c.shape).halfSize; }));
                break;
            case C::ShapeType::Circle:
                f.push_back(label(QObject::tr("Forme"), QObject::tr("Cercle")));
                f.push_back(floatRef<C>(r, e, QObject::tr("Rayon"), [](C& c) -> float& { return std::get<C::Circle>(c.shape).radius; }, 0));
                break;
            case C::ShapeType::Ellipse:
                f.push_back(label(QObject::tr("Forme"), QObject::tr("Ellipse")));
                f.push_back(vecRef<C>(r, e, QObject::tr("Rayons"), [](C& c) -> sf::Vector2f& { return std::get<C::Ellipse>(c.shape).radii; }));
                break;
            case C::ShapeType::Capsule:
                f.push_back(label(QObject::tr("Forme"), QObject::tr("Capsule")));
                f.push_back(floatRef<C>(r, e, QObject::tr("Rayon"), [](C& c) -> float& { return std::get<C::Capsule>(c.shape).radius; }, 0));
                f.push_back(floatRef<C>(r, e, QObject::tr("Hauteur"), [](C& c) -> float& { return std::get<C::Capsule>(c.shape).height; }, 0));
                break;
            case C::ShapeType::Polygon:
                f.push_back(label(QObject::tr("Forme"),
                                  QObject::tr("Polygone (%1 points)").arg(std::get<C::Polygon>(r.getComponent<C>(e).shape).points.size())));
                break;
            }
            f.push_back(vecRef<C>(r, e, QObject::tr("Décalage"), [](C& c) -> sf::Vector2f& { return c.offset; }));
            f.push_back(boolRef<C>(r, e, QObject::tr("Déclencheur"), [](C& c) -> bool& { return c.isTrigger; }));
            f.push_back(floatRef<C>(r, e, QObject::tr("Rebond"), [](C& c) -> float& { return c.restitution; }, 0, 1, 0.05));
            f.push_back(floatRef<C>(r, e, QObject::tr("Friction"), [](C& c) -> float& { return c.friction; }, 0, 1, 0.05));
            return f;
        };
        list.push_back(d);
    }

    // ---------------------------------------------------------------- Camera2d
    {
        ComponentDesc d{ "Camera2d" };
        d.has = [](Registry& r, Entity e) { return r.hasComponent<Camera2d>(e); };
        d.add = [](Registry& r, Entity e) {
            // Le CameraSystem lit la position dans Transform2D : on l'ajoute si besoin
            if (!r.hasComponent<Transform2D>(e)) r.addComponent<Transform2D>(e);
            r.addComponent<Camera2d>(e);
        };
        d.remove = [](Registry& r, Entity e) { r.removeComponent<Camera2d>(e); };
        d.fields = [](Registry& r, Entity e) {
            using C = Camera2d;
            std::vector<FieldDesc> f;
            f.push_back(floatRef<C>(r, e, QObject::tr("Zoom"), [](C& c) -> float& { return c.zoom; }, 0.01, 100, 0.1));
            f.push_back(floatRef<C>(r, e, QObject::tr("Zoom visé"), [](C& c) -> float& { return c.targetZoom; }, 0.01, 100, 0.1));
            f.push_back(floatRef<C>(r, e, QObject::tr("Zoom min"), [](C& c) -> float& { return c.minZoom; }, 0.01, 100, 0.05));
            f.push_back(floatRef<C>(r, e, QObject::tr("Zoom max"), [](C& c) -> float& { return c.maxZoom; }, 0.01, 100, 0.1));
            f.push_back(floatRef<C>(r, e, QObject::tr("Pas de zoom"), [](C& c) -> float& { return c.zoomStep; }, 0, 100, 0.1));
            f.push_back(floatRef<C>(r, e, QObject::tr("Lissage du zoom"), [](C& c) -> float& { return c.zoomLerpSpeed; }, 0, 1000, 1));
            f.push_back(floatRef<C>(r, e, QObject::tr("Vitesse clavier"), [](C& c) -> float& { return c.panSpeedKeyboard; }, 0, 1e6, 10));
            f.push_back(floatRef<C>(r, e, QObject::tr("Vitesse manette"), [](C& c) -> float& { return c.panSpeedGamepad; }, 0, 1e6, 10));
            f.push_back(vecRef<C>(r, e, QObject::tr("Limite min"), [](C& c) -> sf::Vector2f& { return c.worldMin; }, 10));
            f.push_back(vecRef<C>(r, e, QObject::tr("Limite max"), [](C& c) -> sf::Vector2f& { return c.worldMax; }, 10));

            // Entité suivie (targetLock)
            FieldDesc target{ QObject::tr("Suit"), FieldKind::Label };
            target.get = [&r, e] {
                const Entity t = r.getComponent<Camera2d>(e).targetLock;
                if (t == NULL_ENTITY || !r.isValid(t)) return QVariant(QObject::tr("(rien : caméra libre)"));
                if (r.hasComponent<NameComponent>(t) && !r.getComponent<NameComponent>(t).name.empty())
                    return QVariant(QString::fromStdString(r.getComponent<NameComponent>(t).name));
                return QVariant(QObject::tr("Entité %1").arg(t.index));
            };
            f.push_back(target);
            return f;
        };
        list.push_back(d);
    }

    // ---------------------------------------------------------------- Script (comportement)
    {
        /**
         * Describes the Script component.
         *
         * This component acts as a dynamic wrapper around a behavior instance. The
         * script behavior may expose properties via a PROPERTY(...) mechanism. Those
         * runtime properties are read from raw pointers and converted into editor
         * fields. This allows editor-driven editing without hardcoded knowledge of
         * each behavior implementation.
         */
        ComponentDesc d{ "Script" };
        d.has = [](Registry& r, Entity e) { return r.hasComponent<Script>(e); };
        d.remove = [](Registry& r, Entity e) { r.removeComponent<Script>(e); };
        d.fields = [](Registry& r, Entity e) {
            std::vector<FieldDesc> f;
            IBehavior* b = r.getComponent<Script>(e).behavior.get();
            if (!b) { f.push_back(label(QObject::tr("Comportement"), QObject::tr("(aucun)"))); return f; }
            f.push_back(label(QObject::tr("Comportement"), cleanTypeName(typeid(*b).name())));

            // Properties exposed by the behavior instance
            for (size_t i = 0; i < b->m_properties.size(); ++i) {
                const PropertyInfo& p = b->m_properties[i];
                FieldDesc fd{ QString::fromStdString(p.name) };
                void* ptr = p.ptr;
                switch (p.type) {
                case PropertyInfo::Type::Float:
                    fd.kind = FieldKind::Float;
                    fd.get = [ptr] { return QVariant(double(*static_cast<float*>(ptr))); };
                    fd.set = [ptr](const QVariant& v) { *static_cast<float*>(ptr) = float(v.toDouble()); };
                    break;
                case PropertyInfo::Type::Int:
                    fd.kind = FieldKind::Int;
                    fd.get = [ptr] { return QVariant(*static_cast<int*>(ptr)); };
                    fd.set = [ptr](const QVariant& v) { *static_cast<int*>(ptr) = v.toInt(); };
                    break;
                case PropertyInfo::Type::Bool:
                    fd.kind = FieldKind::Bool;
                    fd.get = [ptr] { return QVariant(*static_cast<bool*>(ptr)); };
                    fd.set = [ptr](const QVariant& v) { *static_cast<bool*>(ptr) = v.toBool(); };
                    break;
                case PropertyInfo::Type::Vec2f:
                    fd.kind = FieldKind::Vec2;
                    fd.get = [ptr] { return v2(*static_cast<sf::Vector2f*>(ptr)); };
                    fd.set = [ptr](const QVariant& v) { *static_cast<sf::Vector2f*>(ptr) = toV2(v); };
                    break;
                case PropertyInfo::Type::String:
                    fd.kind = FieldKind::String;
                    fd.get = [ptr] { return QVariant(QString::fromStdString(*static_cast<std::string*>(ptr))); };
                    fd.set = [ptr](const QVariant& v) { *static_cast<std::string*>(ptr) = v.toString().toStdString(); };
                    break;
                }
                f.push_back(fd);
            }
            return f;
        };
        list.push_back(d);
    }

    return list;
}
} // namespace

/**
 * Returns the global list of component descriptors.
 *
 * The list is built once and cached statically so the editor can query it
 * repeatedly without re-creating metadata every time.
 *
 * @return A constant reference to the cached component descriptor list.
 */
const std::vector<ComponentDesc>& componentDescriptors() {
    static const std::vector<ComponentDesc> list = build();
    return list;
}