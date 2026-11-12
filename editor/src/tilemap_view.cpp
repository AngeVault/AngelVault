#include "tilemap_view.h"

#include "components/camera2d.h"
#include "components/collider2d.h"
#include "components/geometry.h"
#include "components/transform2d.h"
#include "systems/render_system.h"
#include "systems/tilemap_render_system.h"
#include "brush_footprint.h"

#include <QApplication>

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <queue>


namespace {
const sf::Color kSelection(255, 170, 40);
const sf::Color kCameraColor(120, 200, 255);
// Taille de l'écran de jeu supposée pour le cadre des caméras (fenêtre par défaut de Game)
const sf::Vector2f kGameScreen(1920.f, 1080.f);

// Rectangle local (avant Transform2D) d'une entité, pour la sélection à la souris et le cadre
sf::FloatRect localBounds(Registry& r, Entity e) {
    if (r.hasComponent<Geometry>(e)) {
        return std::visit([](auto& d) -> sf::FloatRect {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, Geometry::BoxData>) return { -d.halfSize, d.halfSize * 2.f };
            else if constexpr (std::is_same_v<T, Geometry::CircleData>) return { { -d.radius, -d.radius }, { d.radius * 2.f, d.radius * 2.f } };
            else if constexpr (std::is_same_v<T, Geometry::SpriteData>) {
                const sf::Vector2f size(d.rect.size);
                return { -size / 2.f, size };
            } else {
                if (d.points.empty()) return { { -8.f, -8.f }, { 16.f, 16.f } };
                sf::Vector2f mn = d.points[0], mx = d.points[0];
                for (auto p : d.points) { mn.x = std::min(mn.x, p.x); mn.y = std::min(mn.y, p.y); mx.x = std::max(mx.x, p.x); mx.y = std::max(mx.y, p.y); }
                return { mn, mx - mn };
            }
        }, r.getComponent<Geometry>(e).data);
    }
    if (r.hasComponent<Collider2D>(e)) {
        const Collider2D& c = r.getComponent<Collider2D>(e);
        sf::Vector2f half{ 8.f, 8.f };
        switch (c.type()) {
        case Collider2D::ShapeType::Box:     half = std::get<Collider2D::Box>(c.shape).halfSize; break;
        case Collider2D::ShapeType::Circle:  { float rr = std::get<Collider2D::Circle>(c.shape).radius; half = { rr, rr }; break; }
        case Collider2D::ShapeType::Ellipse: half = std::get<Collider2D::Ellipse>(c.shape).radii; break;
        case Collider2D::ShapeType::Capsule: {
            const auto& cap = std::get<Collider2D::Capsule>(c.shape);
            half = { cap.radius, cap.height * 0.5f + cap.radius };
            break;
        }
        default: break;
        }
        return { c.offset - half, half * 2.f };
    }
    return { { -8.f, -8.f }, { 16.f, 16.f } };   // entité sans forme : petit carré
}
}

TilemapView::TilemapView(EditorContext& context, QWidget* parent) : SfmlWidget(parent), m_ctx(context) {
    setMouseTracking(true);
    setAcceptDrops(true);   // prefabs et comportements glissés depuis le panneau Project

    onInit = [this](sf::RenderWindow& target) { init(target); };
    onFrame = [this](sf::RenderWindow& target) { frame(target); };

    connect(&m_ctx, &EditorContext::toolChanged, this, [this] { updateCursor(); });
    // Autre tilemap (ou scène) : on recadre tant que l'utilisateur n'a pas bougé la vue lui-même
    connect(&m_ctx, &EditorContext::documentChanged, this, [this] { m_userCamera = false; m_needsFit = true; });
    updateCursor();
}

TilemapView::~TilemapView() = default;



void TilemapView::init(sf::RenderWindow& target) {
    m_tilemapRenderer = std::make_unique<TilemapRenderSystem>(target, m_textures);
    m_entityRenderer = std::make_unique<RenderSystem>(target, m_textures);
    m_clock.restart();
}

void TilemapView::fitToMap() {
    sf::RenderWindow* target = window();
    if (!target) { m_needsFit = true; return; }
    const sf::Vector2u size = target->getSize();
    if (size.x == 0 || size.y == 0) return;
    // Cadre : la tilemap active ; sans tilemap, les entités de la scène
    sf::FloatRect box;
    if (const TileMapData* map = m_ctx.activeMap(); map && map->width() > 0) {
        box = sf::FloatRect(map->localOrigin() + m_ctx.activeMapOffset(), map->localSize());
    } else {
        Registry& reg = m_ctx.scene();
        bool any = false;
        sf::Vector2f mn, mx;
        for (Entity e : m_ctx.entities()) {
            if (!reg.hasComponent<Transform2D>(e)) continue;
            const sf::Vector2f p = reg.getComponent<Transform2D>(e).getPosition();
            if (!any) { mn = mx = p; any = true; }
            mn = { std::min(mn.x, p.x), std::min(mn.y, p.y) };
            mx = { std::max(mx.x, p.x), std::max(mx.y, p.y) };
        }
        if (!any) { mn = { -320.f, -180.f }; mx = { 320.f, 180.f }; }
        box = sf::FloatRect(mn - sf::Vector2f{ 200.f, 150.f }, mx - mn + sf::Vector2f{ 400.f, 300.f });
    }
    if (box.size.x <= 0.f || box.size.y <= 0.f) return;
    m_camera.zoom = std::clamp(std::min(size.x / box.size.x, size.y / box.size.y) * 0.95f, m_camera.minZoom, m_camera.maxZoom);
    m_camera.position = box.position + box.size * 0.5f;
    m_needsFit = false;
    m_userCamera = false;
}

void TilemapView::resizeEvent(QResizeEvent* e) {
    SfmlWidget::resizeEvent(e);
    if (!m_userCamera) m_needsFit = true;   // les panneaux se placent au démarrage : on garde la carte cadrée
}

void TilemapView::frame(sf::RenderWindow& target) {
    if (!m_tilemapRenderer) return;
    const float dt = std::min(m_clock.restart().asSeconds(), 0.05f);
    if (m_needsFit) fitToMap();

    target.setView(m_camera.makeView(target.getSize()));
    m_tilemapRenderer->onUpdate(m_ctx.scene(), dt);   // toutes les entités Tilemap (aucune = pas de carte)
    if (m_showGrid) drawGrid(target);
    m_entityRenderer->onUpdate(m_ctx.scene(), dt);
    drawColliders(target);
    drawCameras(target);
    drawSelection(target);
    drawResizeGizmo(target);
    drawGizmo(target);
    drawBrush(target);
}

void TilemapView::drawGrid(sf::RenderTarget& target) {
    const TileMapData* map = m_ctx.activeMap();
    if (!map || map->width() <= 0 || map->height() <= 0) return;
    if (m_camera.zoom * std::min(map->tileSize.x, map->tileSize.y) < 6.f) return;   // trop dézoomé : illisible
    const sf::Vector2f o = m_ctx.activeMapOffset();
    const bool iso = map->orientation == TileMapOrientation::Isometric;
    const sf::Vector2f ts = map->tileSize;
    // Sommet (gx, gy) de la grille, dans le monde
    auto vertex = [&](int gx, int gy) {
        return o + (iso ? sf::Vector2f{ (gx - gy) * ts.x * 0.5f, (gx + gy) * ts.y * 0.5f }
                        : sf::Vector2f{ gx * ts.x, gy * ts.y });
    };
    const sf::Color c(0, 0, 0, 55);
    sf::VertexArray lines(sf::PrimitiveType::Lines);
    for (int x = 0; x <= map->width(); ++x) { lines.append({ vertex(x, 0), c }); lines.append({ vertex(x, map->height()), c }); }
    for (int y = 0; y <= map->height(); ++y) { lines.append({ vertex(0, y), c }); lines.append({ vertex(map->width(), y), c }); }
    target.draw(lines);
}

// Cadre de ce que voit chaque caméra du jeu (entité avec Camera2d) + petite icône à sa position
void TilemapView::drawCameras(sf::RenderTarget& target) {
    Registry& reg = m_ctx.scene();
    const float px = 1.f / m_camera.zoom;   // 1 pixel écran en unités monde
    reg.view<Camera2d, Transform2D>([&](Entity, Camera2d& cam, Transform2D& t) {
        const float zoom = cam.zoom > 0.f ? cam.zoom : 1.f;
        const sf::Vector2f size = kGameScreen / zoom;
        sf::RectangleShape frame(size);
        frame.setOrigin(size / 2.f);
        frame.setPosition(t.getPosition());
        frame.setFillColor(sf::Color(120, 200, 255, 12));
        frame.setOutlineColor(sf::Color(120, 200, 255, 170));
        frame.setOutlineThickness(1.5f * px);
        target.draw(frame);

        // icône : un boîtier et un objectif, taille fixe à l'écran
        sf::RectangleShape body({ 18.f * px, 12.f * px });
        body.setOrigin({ 11.f * px, 6.f * px });
        body.setPosition(t.getPosition());
        body.setFillColor(kCameraColor);
        target.draw(body);
        sf::ConvexShape lens(3);
        lens.setPoint(0, { 7.f * px, 0.f });
        lens.setPoint(1, { 13.f * px, -6.f * px });
        lens.setPoint(2, { 13.f * px, 6.f * px });
        lens.setPosition(t.getPosition());
        lens.setFillColor(kCameraColor);
        target.draw(lens);
    });
}

void TilemapView::drawSelection(sf::RenderTarget& target) {
    Registry& reg = m_ctx.scene();
    const Entity e = m_ctx.selected();
    if (!m_ctx.isValid(e) || !reg.hasComponent<Transform2D>(e)) return;
    const Transform2D& t = reg.getComponent<Transform2D>(e);
    const sf::FloatRect b = localBounds(reg, e);
    const float px = 1.f / m_camera.zoom;            // 1 pixel écran en unités monde

    sf::RectangleShape frame(b.size);
    frame.setPosition(b.position);
    frame.setFillColor(sf::Color(255, 170, 40, 25));
    frame.setOutlineColor(kSelection);
    frame.setOutlineThickness(2.f * px);
    target.draw(frame, sf::RenderStates(t.getLocalMatrix()));

    sf::CircleShape pivot(3.f * px);                  // point d'origine de l'entité
    pivot.setOrigin({ 3.f * px, 3.f * px });
    pivot.setPosition(t.getPosition());
    pivot.setFillColor(kSelection);
    target.draw(pivot);
}

// ---- Gizmo de déplacement ------------------------------------------------------------
// Taille fixe à l'écran (en pixels), quel que soit le zoom.
namespace {
constexpr float kGizmoLength = 70.f;    // longueur d'une flèche
constexpr float kGizmoHead   = 14.f;    // taille de la pointe
constexpr float kGizmoGrab   = 7.f;     // tolérance de clic autour d'une flèche
constexpr float kGizmoBox    = 9.f;     // demi-côté du carré central
const sf::Color kGizmoX(235, 75, 75);
const sf::Color kGizmoY(95, 205, 95);
const sf::Color kGizmoFree(90, 160, 255, 150);
const sf::Color kGizmoHot(255, 215, 60);
}

TilemapView::GizmoHandle TilemapView::gizmoHandleAt(sf::Vector2f world) const {
    if (m_ctx.tool() != EditorTool::Select) return GizmoHandle::None;
    Registry& reg = m_ctx.scene();
    const Entity e = m_ctx.selected();
    if (!m_ctx.isValid(e) || !reg.hasComponent<Transform2D>(e)) return GizmoHandle::None;
    if (m_ctx.isPositionLocked(e)) return GizmoHandle::None;   // caméra liée : pas de gizmo

    const sf::Vector2f d = (world - reg.getComponent<Transform2D>(e).getPosition()) * m_camera.zoom;   // en pixels écran
    if (std::abs(d.x) <= kGizmoBox && std::abs(d.y) <= kGizmoBox) return GizmoHandle::Free;
    if (std::abs(d.y) <= kGizmoGrab && d.x > kGizmoBox && d.x <= kGizmoLength + kGizmoHead) return GizmoHandle::X;
    if (std::abs(d.x) <= kGizmoGrab && d.y > kGizmoBox && d.y <= kGizmoLength + kGizmoHead) return GizmoHandle::Y;
    return GizmoHandle::None;
}

void TilemapView::drawGizmo(sf::RenderTarget& target) {
    if (m_ctx.tool() != EditorTool::Select) return;
    Registry& reg = m_ctx.scene();
    const Entity e = m_ctx.selected();
    if (!m_ctx.isValid(e) || !reg.hasComponent<Transform2D>(e)) return;

    const sf::Vector2f o = reg.getComponent<Transform2D>(e).getPosition();
    const float px = 1.f / m_camera.zoom;

    if (m_ctx.isPositionLocked(e)) {   // caméra liée à une entité : cadenas à la place du gizmo
        const sf::Color lockColor(200, 200, 210, 230);
        const sf::Vector2f at = o + sf::Vector2f(16.f, -16.f) * px;
        sf::CircleShape shackle(5.f * px);                // anse
        shackle.setOrigin({ 5.f * px, 5.f * px });
        shackle.setPosition(at + sf::Vector2f(0.f, -4.f) * px);
        shackle.setFillColor(sf::Color::Transparent);
        shackle.setOutlineThickness(2.f * px);
        shackle.setOutlineColor(lockColor);
        target.draw(shackle);
        sf::RectangleShape body({ 14.f * px, 10.f * px }); // corps
        body.setOrigin({ 7.f * px, 2.f * px });
        body.setPosition(at);
        body.setFillColor(lockColor);
        target.draw(body);
        return;
    }

    const GizmoHandle hot = (m_dragHandle != GizmoHandle::None) ? m_dragHandle : m_hoverHandle;

    // dir : (1,0) pour X, (0,1) pour Y (l'axe Y du monde descend, comme dans SFML)
    auto arrow = [&](sf::Vector2f dir, sf::Color color) {
        const sf::Vector2f side(-dir.y, dir.x);
        sf::ConvexShape shaft(4);
        const float w = 1.5f * px;
        const sf::Vector2f a = o + dir * (kGizmoBox * px), b = o + dir * (kGizmoLength * px);
        shaft.setPoint(0, a + side * w); shaft.setPoint(1, b + side * w);
        shaft.setPoint(2, b - side * w); shaft.setPoint(3, a - side * w);
        shaft.setFillColor(color);
        target.draw(shaft);

        sf::ConvexShape head(3);
        head.setPoint(0, o + dir * ((kGizmoLength + kGizmoHead) * px));
        head.setPoint(1, b + side * (kGizmoHead * 0.45f * px));
        head.setPoint(2, b - side * (kGizmoHead * 0.45f * px));
        head.setFillColor(color);
        target.draw(head);
    };
    arrow({ 1.f, 0.f }, hot == GizmoHandle::X ? kGizmoHot : kGizmoX);
    arrow({ 0.f, 1.f }, hot == GizmoHandle::Y ? kGizmoHot : kGizmoY);

    sf::RectangleShape box({ 2.f * kGizmoBox * px, 2.f * kGizmoBox * px });
    box.setOrigin({ kGizmoBox * px, kGizmoBox * px });
    box.setPosition(o);
    box.setFillColor(hot == GizmoHandle::Free ? sf::Color(255, 215, 60, 150) : kGizmoFree);
    box.setOutlineThickness(1.f * px);
    box.setOutlineColor(hot == GizmoHandle::Free ? kGizmoHot : sf::Color(90, 160, 255));
    target.draw(box);
}

void TilemapView::drawBrush(sf::RenderTarget& target) {
    if (!m_hoverValid || !underMouse()) return;
    const TileMapData* map = m_ctx.activeMap();
    const EditorTool tool = m_ctx.tool();
    if (!map || tool == EditorTool::Select || tool == EditorTool::Resize) return;

    const sf::Vector2f o = m_ctx.activeMapOffset();
    const float px = 1.f / m_camera.zoom;
    auto outlineCell = [&](sf::Vector2i c, sf::Color fill, sf::Color outline, float thickness) {
        if (!map->inBounds(c.x, c.y)) return;
        const auto corners = map->cellCorners(c.x, c.y);
        sf::ConvexShape cell(4);
        for (int i = 0; i < 4; ++i) cell.setPoint(i, corners[i] + o);
        cell.setFillColor(fill);
        cell.setOutlineColor(outline);
        cell.setOutlineThickness(-thickness * px);
        target.draw(cell);
    };
    // Isométrique : de l'arrière vers l'avant, pour que les aperçus se recouvrent comme les vraies tiles
    auto sortBackToFront = [](std::vector<sf::Vector2i>& cells) {
        std::stable_sort(cells.begin(), cells.end(), [](sf::Vector2i a, sf::Vector2i b) { return a.x + a.y < b.x + b.y; });
    };

    if (tool == EditorTool::Pick || tool == EditorTool::Fill) {
        const sf::Color c = tool == EditorTool::Pick ? sf::Color(120, 200, 255, 230) : sf::Color(255, 210, 90, 230);
        outlineCell(m_hoveredTile, sf::Color(c.r, c.g, c.b, 35), c, 2.f);
        return;
    }

    const bool erase = tool == EditorTool::Erase;
    const TileBrush& brush = m_ctx.brush();
    const Tileset* ts = map->tileset(brush.slot);
    const sf::Color fill = erase ? sf::Color(255, 60, 60, 45) : sf::Color(255, 255, 255, 25);
    const sf::Color outline = erase ? sf::Color(255, 90, 90, 210) : sf::Color(255, 255, 255, 190);
    const sf::Color ghost(255, 255, 255, 150);
    TilemapRenderer& renderer = m_tilemapRenderer->renderer();

    // Maj enfoncée : la ligne qui sera tracée depuis le dernier point
    if ((QApplication::keyboardModifiers() & Qt::ShiftModifier) && m_hasStrokeEnd && !m_painting)
        for (sf::Vector2i c : cellLine(m_strokeEnd, m_hoveredTile)) outlineCell(c, sf::Color(255, 255, 255, 18), sf::Color(255, 255, 255, 90), 1.f);

    if (!erase && ts && brush.isStamp()) {   // tampon : le bloc de tiles, coin haut-gauche sur la case visée
        std::vector<sf::Vector2i> cells;
        const sf::Vector2i first = ts->tileCoords(brush.tile);
        for (int j = 0; j < brush.height; ++j)
            for (int i = 0; i < brush.width; ++i) cells.push_back(m_hoveredTile + sf::Vector2i{ i, j });
        sortBackToFront(cells);
        for (sf::Vector2i c : cells) {
            if (!map->inBounds(c.x, c.y)) continue;
            const sf::Vector2i d = c - m_hoveredTile;
            const int index = ts->tileIndex(first.x + d.x, first.y + d.y);
            if (index >= 0) renderer.drawGhost(target, *map, o, brush.slot, index, c.x, c.y, ghost);
        }
        for (sf::Vector2i c : cells) outlineCell(c, sf::Color::Transparent, outline, 1.5f);
        return;
    }

    std::vector<sf::Vector2i> cells;
    for (sf::Vector2i d : brushFootprint(m_ctx.brushSize(), m_ctx.brushShape())) cells.push_back(m_hoveredTile + d);
    sortBackToFront(cells);
    if (!erase && ts) {
        const int tile = brush.autotile ? int(kAutoTile) : brush.tile;
        // Grand pinceau : l'aperçu des tiles seulement au centre (sinon on ne voit plus la carte)
        if (cells.size() <= 64) {
            for (sf::Vector2i c : cells)
                if (map->inBounds(c.x, c.y)) renderer.drawGhost(target, *map, o, brush.slot, tile, c.x, c.y, ghost);
        } else {
            renderer.drawGhost(target, *map, o, brush.slot, tile, m_hoveredTile.x, m_hoveredTile.y, ghost);
        }
    }
    for (sf::Vector2i c : cells) outlineCell(c, fill, outline, cells.size() > 1 ? 1.f : 1.5f);
}

void TilemapView::updateCursor() {
    if (m_panning) return;
    if (m_ctx.tool() == EditorTool::Select || m_ctx.tool() == EditorTool::Resize) unsetCursor();
    else setCursor(Qt::CrossCursor);
}

sf::Vector2f TilemapView::toWorld(const QPointF& p) {
    const qreal dpr = devicePixelRatioF();
    const sf::Vector2u size = window() ? window()->getSize() : sf::Vector2u(1, 1);
    const sf::Vector2f half = { size.x * 0.5f, size.y * 0.5f };
    const sf::Vector2f px = { float(p.x() * dpr), float(p.y() * dpr) };
    return m_camera.position + (px - half) / m_camera.zoom;
}

// Pas de l'aimantation (Ctrl) : demi-case de la tilemap active, 16 sans tilemap
float TilemapView::snapStep() {
    const TileMapData* map = m_ctx.activeMap();
    return map ? std::max(1.f, std::min(map->tileSize.x, map->tileSize.y) * 0.5f) : 16.f;
}

Entity TilemapView::pickEntity(sf::Vector2f world) {
    Registry& reg = m_ctx.scene();
    Entity best = NULL_ENTITY;
    int bestLayer = INT32_MIN;
    for (Entity e : m_ctx.entities()) {
        if (!reg.hasComponent<Transform2D>(e) || reg.hasComponent<Tilemap>(e)) continue;   // tilemap : via la Hierarchy
        const sf::Vector2f local = reg.getComponent<Transform2D>(e).getLocalMatrix().getInverse().transformPoint(world);
        if (!localBounds(reg, e).contains(local)) continue;
        const int layer = reg.hasComponent<Geometry>(e) ? reg.getComponent<Geometry>(e).layer : 0;
        if (layer >= bestLayer) { best = e; bestLayer = layer; }   // à égalité : la plus récente (dessinée au-dessus)
    }
    return best;
}

// ---- Collisions et outil Taille -----------------------------------------------------------------

namespace {
// Poignées : coins et milieux des bords, dans le sens des aiguilles d'une montre depuis le haut-gauche
const sf::Vector2i kResizeDirs[8] = { { -1, -1 }, { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 } };
constexpr float kResizeHandle = 5.f;   // demi-côté d'une poignée (px écran)
const sf::Color kColliderColor(80, 220, 120);
const sf::Color kTriggerColor(240, 200, 60);

void drawPolyline(sf::RenderTarget& target, const std::vector<sf::Vector2f>& pts, const sf::Transform& m,
                  sf::Color color, bool fill) {
    if (pts.size() < 2) return;
    if (fill) {
        sf::VertexArray fan(sf::PrimitiveType::TriangleFan);
        sf::Vector2f c;
        for (sf::Vector2f p : pts) c += p;
        c /= float(pts.size());
        const sf::Color f(color.r, color.g, color.b, 28);
        fan.append({ m.transformPoint(c), f });
        for (sf::Vector2f p : pts) fan.append({ m.transformPoint(p), f });
        fan.append({ m.transformPoint(pts.front()), f });
        target.draw(fan);
    }
    sf::VertexArray line(sf::PrimitiveType::LineStrip);
    for (sf::Vector2f p : pts) line.append({ m.transformPoint(p), color });
    line.append({ m.transformPoint(pts.front()), color });
    target.draw(line);
}
} // namespace

void TilemapView::drawColliders(sf::RenderTarget& target) {
    Registry& reg = m_ctx.scene();
    const bool resizing = m_ctx.tool() == EditorTool::Resize;
    if (!m_showColliders && !resizing) return;
    for (Entity e : m_ctx.entities()) {
        if (!reg.hasComponent<Collider2D>(e) || !reg.hasComponent<Transform2D>(e)) continue;
        if (!m_showColliders && e != m_ctx.selected()) continue;   // Collisions masquées : seulement celle qu'on redimensionne
        const Collider2D& c = reg.getComponent<Collider2D>(e);
        const sf::Color color = c.isTrigger ? kTriggerColor : kColliderColor;
        drawPolyline(target, shape_edit::colliderOutline(c), shape_edit::colliderToWorld(reg.getComponent<Transform2D>(e), c),
                     color, c.type() != Collider2D::ShapeType::Polygon);
    }
}

TilemapView::ResizeFrame TilemapView::resizeFrame(Entity e) const {
    ResizeFrame f;
    Registry& reg = m_ctx.scene();
    if (!m_ctx.isValid(e) || !reg.hasComponent<Transform2D>(e)) return f;
    const Transform2D& t = reg.getComponent<Transform2D>(e);
    const ResizeTarget target = m_ctx.resizeTarget();
    if (target != ResizeTarget::Collider && reg.hasComponent<Geometry>(e)) {
        f.valid = true;
        f.box = shape_edit::geometryBox(reg.getComponent<Geometry>(e));
        f.toWorld = t.getLocalMatrix();
    } else if (target != ResizeTarget::Geometry && reg.hasComponent<Collider2D>(e)) {
        const Collider2D& c = reg.getComponent<Collider2D>(e);
        f.valid = true;
        f.collider = true;
        f.box = shape_edit::colliderBox(c);
        f.toWorld = shape_edit::colliderToWorld(t, c);
    }
    return f;
}

int TilemapView::resizeHandleAt(sf::Vector2f world) const {
    if (m_ctx.tool() != EditorTool::Resize) return -1;
    const ResizeFrame f = resizeFrame(m_ctx.selected());
    if (!f.valid) return -1;
    const float grab = (kResizeHandle + 3.f) / m_camera.zoom;
    for (int i = 0; i < 8; ++i) {
        const sf::Vector2f d(kResizeDirs[i]);
        const sf::Vector2f p = f.toWorld.transformPoint(f.box.center + sf::Vector2f{ d.x * f.box.half.x, d.y * f.box.half.y });
        if (std::abs(world.x - p.x) <= grab && std::abs(world.y - p.y) <= grab) return i;
    }
    return -1;
}

void TilemapView::beginResize(int handle) {
    Registry& reg = m_ctx.scene();
    const Entity e = m_ctx.selected();
    m_resize = ResizeDrag{};
    m_resize.frame = resizeFrame(e);
    if (!m_resize.frame.valid) return;
    m_resize.handle = handle;
    m_resize.entity = e;
    const ResizeTarget target = m_ctx.resizeTarget();
    // La forme n'est redimensionnée que si elle sert de référence (sinon c'est « Collision » seule)
    m_resize.hasGeometry = target != ResizeTarget::Collider && !m_resize.frame.collider && reg.hasComponent<Geometry>(e);
    m_resize.hasCollider = target != ResizeTarget::Geometry && reg.hasComponent<Collider2D>(e);
    if (m_resize.hasGeometry) m_resize.geometry = reg.getComponent<Geometry>(e);
    if (m_resize.hasCollider) m_resize.collider = reg.getComponent<Collider2D>(e);
    m_resize.scale = reg.getComponent<Transform2D>(e).getScale();
}

void TilemapView::dragResize(sf::Vector2f world, Qt::KeyboardModifiers mods) {
    Registry& reg = m_ctx.scene();
    const Entity e = m_resize.entity;
    if (!m_ctx.isValid(e) || !reg.hasComponent<Transform2D>(e)) { m_resize.handle = -1; return; }
    const ResizeFrame& f = m_resize.frame;
    const sf::Vector2i dir = kResizeDirs[m_resize.handle];
    const sf::Vector2f d(dir);
    const sf::Vector2f half = { std::max(f.box.half.x, 0.5f), std::max(f.box.half.y, 0.5f) };

    // Point fixe : le bord opposé pour une collision seule (comme Unity), sinon le centre de la forme
    const bool colliderOnly = f.collider && m_ctx.resizeTarget() == ResizeTarget::Collider;
    const sf::Vector2f anchor = colliderOnly ? f.box.center - sf::Vector2f{ d.x * half.x, d.y * half.y } : f.box.center;
    const sf::Vector2f handle0 = f.box.center + sf::Vector2f{ d.x * half.x, d.y * half.y };
    const sf::Vector2f m = f.toWorld.getInverse().transformPoint(world);

    sf::Vector2f s{ 1.f, 1.f };
    if (dir.x != 0 && std::abs(handle0.x - anchor.x) > 1e-3f) s.x = (m.x - anchor.x) / (handle0.x - anchor.x);
    if (dir.y != 0 && std::abs(handle0.y - anchor.y) > 1e-3f) s.y = (m.y - anchor.y) / (handle0.y - anchor.y);
    if (mods & Qt::ShiftModifier) {   // Maj : garder les proportions
        const float u = dir.x != 0 && dir.y != 0 ? std::max(s.x, s.y) : (dir.x != 0 ? s.x : s.y);
        s = { u, u };
    }
    if (mods & Qt::ControlModifier) {   // Ctrl : taille arrondie à la demi-case
        const float step = snapStep();
        auto snap = [&](float h, float k) { return std::max(step, std::round(h * k * 2.f / step) * step) / (2.f * h); };
        if (dir.x != 0 || (mods & Qt::ShiftModifier)) s.x = snap(half.x, s.x);
        if (dir.y != 0 || (mods & Qt::ShiftModifier)) s.y = snap(half.y, s.y);
    }
    // Jamais retournée ni plus petite qu'un pixel
    s.x = std::max(s.x, 1.f / half.x);
    s.y = std::max(s.y, 1.f / half.y);

    Transform2D& t = reg.getComponent<Transform2D>(e);
    if (m_resize.hasGeometry && reg.hasComponent<Geometry>(e))
        shape_edit::scaleGeometry(reg.getComponent<Geometry>(e), m_resize.geometry, t, m_resize.scale, s, anchor, dir);
    if (m_resize.hasCollider && reg.hasComponent<Collider2D>(e)) {
        Collider2D& c = reg.getComponent<Collider2D>(e);
        // point fixe dans le repère de la collision
        const sf::Vector2f a = f.collider ? anchor
            : shape_edit::colliderToWorld(t, m_resize.collider).getInverse().transformPoint(f.toWorld.transformPoint(anchor));
        shape_edit::scaleCollider(c, m_resize.collider, s, a, dir);
    }
    m_ctx.notifyEntityModified(e, this);
    const sf::Vector2f size{ half.x * s.x * 2.f, half.y * s.y * 2.f };
    emit statusText(tr("Taille : %1 x %2").arg(double(size.x), 0, 'f', 1).arg(double(size.y), 0, 'f', 1));
}

void TilemapView::drawResizeGizmo(sf::RenderTarget& target) {
    if (m_ctx.tool() != EditorTool::Resize) return;
    const ResizeFrame f = m_resize.handle >= 0 ? resizeFrame(m_resize.entity) : resizeFrame(m_ctx.selected());
    if (!f.valid) return;
    const float px = 1.f / m_camera.zoom;
    const ResizeTarget mode = m_ctx.resizeTarget();
    const sf::Color color = mode == ResizeTarget::Both ? sf::Color(240, 240, 240) : f.collider ? kColliderColor : kSelection;

    const sf::Vector2f c = f.box.center, h = f.box.half;
    drawPolyline(target, { c + sf::Vector2f{ -h.x, -h.y }, c + sf::Vector2f{ h.x, -h.y }, c + sf::Vector2f{ h.x, h.y }, c + sf::Vector2f{ -h.x, h.y } },
                 f.toWorld, color, false);
    const int hot = m_resize.handle >= 0 ? m_resize.handle : m_hoverResize;
    for (int i = 0; i < 8; ++i) {
        const sf::Vector2f d(kResizeDirs[i]);
        sf::RectangleShape handle({ 2.f * kResizeHandle * px, 2.f * kResizeHandle * px });
        handle.setOrigin({ kResizeHandle * px, kResizeHandle * px });
        handle.setPosition(f.toWorld.transformPoint(c + sf::Vector2f{ d.x * h.x, d.y * h.y }));
        handle.setFillColor(i == hot ? sf::Color(255, 215, 60) : sf::Color(30, 30, 34));
        handle.setOutlineThickness(1.5f * px);
        handle.setOutlineColor(i == hot ? sf::Color(255, 215, 60) : color);
        target.draw(handle);
    }
}

// ---- Peinture ------------------------------------------------------------------------------------

QString TilemapView::describeCell(const TileMapData& map, TileCell cell) const {
    const Tileset* ts = map.tilesetOf(cell);
    if (!ts) return tr("Vide");
    const QString name = QString::fromStdString(ts->name);
    return cellIsAuto(cell) ? tr("%1 (autotile)").arg(name) : tr("%1, tile %2").arg(name).arg(cellTile(cell));
}

void TilemapView::pickAt(sf::Vector2i cell) {
    const TileMapData* map = m_ctx.activeMap();
    if (!map) return;
    const TileCell c = map->get(cell.x, cell.y);
    if (!map->tilesetOf(c)) { emit statusText(tr("Pipette : case vide")); return; }
    TileBrush b;
    b.slot = cellSlot(c);
    b.autotile = cellIsAuto(c);
    b.tile = b.autotile ? map->tileset(b.slot)->previewTile() : cellTile(c);
    m_ctx.setBrush(b);
    m_ctx.setTool(EditorTool::Paint);
    emit statusText(tr("Pipette : %1").arg(describeCell(*map, c)));
}

bool TilemapView::dab(sf::Vector2i cell) {
    TileMapData* map = m_ctx.activeMap();
    if (!map) return false;
    const bool erase = m_ctx.tool() == EditorTool::Erase;
    const TileBrush& brush = m_ctx.brush();
    const Tileset* ts = map->tileset(brush.slot);
    if (!erase && !ts) return false;

    bool changed = false;
    if (!erase && brush.isStamp()) {
        // Tampon : posé sur une grille qui part du début du tracé (glisser = motif répété sans chevauchement)
        const sf::Vector2i aligned = m_strokeOrigin + sf::Vector2i{
            floorDiv(cell.x - m_strokeOrigin.x, brush.width) * brush.width,
            floorDiv(cell.y - m_strokeOrigin.y, brush.height) * brush.height };
        if (aligned == m_lastDab) return false;
        m_lastDab = aligned;
        const sf::Vector2i first = ts->tileCoords(brush.tile);
        for (int j = 0; j < brush.height; ++j)
            for (int i = 0; i < brush.width; ++i) {
                const int index = ts->tileIndex(first.x + i, first.y + j);
                if (index >= 0) changed |= map->set(aligned.x + i, aligned.y + j, makeCell(brush.slot, index));
            }
        return changed;
    }
    const TileCell value = erase ? 0 : makeCell(brush.slot, brush.autotile ? int(kAutoTile) : brush.tile);
    for (sf::Vector2i d : brushFootprint(m_ctx.brushSize(), m_ctx.brushShape()))
        changed |= map->set(cell.x + d.x, cell.y + d.y, value);   // hors carte : ignoré
    return changed;
}

void TilemapView::beginStroke(sf::Vector2i cell) {
    m_strokeOrigin = cell;
    m_lastDab = { INT32_MIN, INT32_MIN };
    m_lastPainted = cell;
    m_strokeChanged = dab(cell);
}

void TilemapView::strokeTo(sf::Vector2i cell) {
    if (cell == m_lastPainted) return;
    // Toutes les cases entre la position précédente et celle-ci : un geste rapide ne laisse pas de trous
    const std::vector<sf::Vector2i> line = cellLine(m_lastPainted, cell);
    for (std::size_t i = 1; i < line.size(); ++i) m_strokeChanged |= dab(line[i]);
    m_lastPainted = cell;
    if (m_strokeChanged) m_ctx.notifyMapEdited();
}

// Remplir : la zone de cases identiques (4 voisines) sous la souris prend le pinceau
void TilemapView::floodFill(sf::Vector2i start) {
    TileMapData* map = m_ctx.activeMap();
    if (!map || !map->inBounds(start.x, start.y)) return;
    const TileBrush& brush = m_ctx.brush();
    const Tileset* ts = map->tileset(brush.slot);
    if (!ts) return;
    const TileCell target = map->get(start.x, start.y);
    const sf::Vector2i first = ts->tileCoords(brush.tile);
    auto valueAt = [&](sf::Vector2i c) -> TileCell {
        if (brush.autotile) return makeCell(brush.slot, kAutoTile);
        if (!brush.isStamp()) return makeCell(brush.slot, brush.tile);
        // tampon : motif répété à partir de la case cliquée
        const int i = ((c.x - start.x) % brush.width + brush.width) % brush.width;
        const int j = ((c.y - start.y) % brush.height + brush.height) % brush.height;
        const int index = ts->tileIndex(first.x + i, first.y + j);
        return index >= 0 ? makeCell(brush.slot, index) : target;
    };
    if (!brush.isStamp() && valueAt(start) == target) return;

    std::vector<uint8_t> seen(std::size_t(map->width()) * map->height(), 0);
    std::queue<sf::Vector2i> todo;
    todo.push(start);
    seen[std::size_t(start.y) * map->width() + start.x] = 1;
    int count = 0;
    bool changed = false;
    while (!todo.empty()) {
        const sf::Vector2i c = todo.front();
        todo.pop();
        changed |= map->set(c.x, c.y, valueAt(c));
        ++count;
        for (sf::Vector2i d : kTileEdgeDirs) {
            const sf::Vector2i n = c + d;
            if (!map->inBounds(n.x, n.y)) continue;
            uint8_t& s = seen[std::size_t(n.y) * map->width() + n.x];
            if (s || map->get(n.x, n.y) != target) continue;
            s = 1;
            todo.push(n);
        }
    }
    if (changed) {
        m_ctx.notifyMapEdited();
        emit statusText(tr("Remplir : %1 case(s)").arg(count));
    }
}

void TilemapView::changeBrushSize(int delta) {
    m_ctx.setBrushSize(m_ctx.brushSize() + delta);
    const int n = m_ctx.brushSize();
    emit statusText(tr("Pinceau : %1 x %1 (%2)").arg(n)
                        .arg(m_ctx.brushShape() == BrushShape::Circle ? tr("rond") : tr("carré")));
}

void TilemapView::updateHover(const QPointF& pos) {
    const TileMapData* map = m_ctx.activeMap();
    if (!map) {
        if (m_hoverValid) { m_hoverValid = false; emit hoveredTileChanged(-1, -1, QString()); }
        return;
    }
    const sf::Vector2i tile = map->cellAt(toWorld(pos) - m_ctx.activeMapOffset());
    const bool valid = map->inBounds(tile.x, tile.y);
    if (valid == m_hoverValid && tile == m_hoveredTile) return;
    m_hoveredTile = tile;
    m_hoverValid = valid;
    emit hoveredTileChanged(valid ? tile.x : -1, valid ? tile.y : -1,
                            valid ? describeCell(*map, map->get(tile.x, tile.y)) : QString());
}

void TilemapView::wheelEvent(QWheelEvent* e) {
    // Ctrl + molette : taille du pinceau (la molette seule zoome)
    if (e->modifiers() & Qt::ControlModifier) {
        const int d = e->angleDelta().y() != 0 ? e->angleDelta().y() : e->angleDelta().x();
        if (d != 0 && m_ctx.tool() != EditorTool::Select) changeBrushSize(d > 0 ? 1 : -1);
        e->accept();
        return;
    }
    const float steps = e->angleDelta().y() / 120.f;
    if (steps == 0.f) return;
    const sf::Vector2f before = toWorld(e->position());
    m_camera.zoom = std::clamp(m_camera.zoom * std::pow(1.15f, steps), m_camera.minZoom, m_camera.maxZoom);
    const sf::Vector2f after = toWorld(e->position());
    m_camera.position += before - after;   // le point sous la souris ne bouge pas
    m_userCamera = true;
    e->accept();
}

void TilemapView::mousePressEvent(QMouseEvent* e) {
    setFocus();
    if (e->button() == Qt::MiddleButton || e->button() == Qt::RightButton) {
        m_panning = true;
        m_lastMouse = e->position();
        setCursor(Qt::ClosedHandCursor);
    } else if (e->button() == Qt::LeftButton) {
        m_ctx.beginInteraction();   // tout le geste (glisser, tracé...) = une seule étape d'annulation
        m_interacting = true;
        const sf::Vector2f world = toWorld(e->position());
        if (m_ctx.tool() == EditorTool::Select) {
            // Le gizmo passe avant le sprite : on peut le saisir même s'il dépasse sur une autre entité
            const GizmoHandle handle = gizmoHandleAt(world);
            const Entity hit = (handle != GizmoHandle::None) ? m_ctx.selected() : pickEntity(world);
            if (hit != m_ctx.selected()) m_ctx.select(hit);
            if (m_ctx.isValid(hit) && m_ctx.isPositionLocked(hit)) {
                setCursor(Qt::ForbiddenCursor);   // caméra liée : on la sélectionne, mais elle ne bouge pas
            } else if (m_ctx.isValid(hit)) {
                m_dragEntity = hit;
                m_dragHandle = handle;
                m_dragStart = m_ctx.scene().getComponent<Transform2D>(hit).getPosition();
                m_dragOffset = m_dragStart - world;
            }
        } else if (m_ctx.tool() == EditorTool::Resize) {
            // Une poignée de la sélection, sinon on sélectionne ce qui est sous la souris
            const int handle = resizeHandleAt(world);
            if (handle >= 0) beginResize(handle);
            else if (const Entity hit = pickEntity(world); hit != m_ctx.selected()) m_ctx.select(hit);
        } else {
            updateHover(e->position());
            const EditorTool tool = m_ctx.tool();
            TileMapData* map = m_ctx.activeMap();
            if (m_hoverValid && map) {
                if (tool == EditorTool::Pick || (e->modifiers() & Qt::AltModifier)) {
                    pickAt(m_hoveredTile);                       // Alt + clic : pipette rapide
                } else if (tool == EditorTool::Paint && !map->tileset(m_ctx.brush().slot)) {
                    emit statusText(tr("Rien à peindre : choisissez une tile dans la Tile Palette (ou ajoutez un tileset)"));
                } else if (tool == EditorTool::Fill) {
                    if (!map->tileset(m_ctx.brush().slot)) emit statusText(tr("Rien à peindre : choisissez une tile dans la Tile Palette"));
                    else floodFill(m_hoveredTile);
                } else if ((e->modifiers() & Qt::ShiftModifier) && m_hasStrokeEnd) {
                    // Maj + clic : ligne droite depuis la fin du tracé précédent
                    beginStroke(m_strokeEnd);
                    strokeTo(m_hoveredTile);
                    m_painting = true;
                } else {
                    beginStroke(m_hoveredTile);
                    if (m_strokeChanged) m_ctx.notifyMapEdited();
                    m_painting = true;
                }
            }
        }
    }
    SfmlWidget::mousePressEvent(e);
}

void TilemapView::mouseMoveEvent(QMouseEvent* e) {
    if (m_panning) {
        const QPointF d = (e->position() - m_lastMouse) * devicePixelRatioF();
        m_camera.position -= sf::Vector2f(float(d.x()), float(d.y())) / m_camera.zoom;
        m_lastMouse = e->position();
        m_userCamera = true;
    }
    updateHover(e->position());

    if (m_resize.handle >= 0 && (e->buttons() & Qt::LeftButton)) {   // outil Taille : poignée tenue
        dragResize(toWorld(e->position()), e->modifiers());
        SfmlWidget::mouseMoveEvent(e);
        return;
    }
    if (m_ctx.tool() == EditorTool::Resize) {   // poignée survolée : surbrillance + curseur
        const int h = resizeHandleAt(toWorld(e->position()));
        if (h != m_hoverResize) {
            m_hoverResize = h;
            static const Qt::CursorShape kCursors[8] = { Qt::SizeFDiagCursor, Qt::SizeVerCursor, Qt::SizeBDiagCursor, Qt::SizeHorCursor,
                                                         Qt::SizeFDiagCursor, Qt::SizeVerCursor, Qt::SizeBDiagCursor, Qt::SizeHorCursor };
            if (h < 0) updateCursor();
            else setCursor(kCursors[h]);
        }
    }

    if (!m_ctx.isValid(m_dragEntity)) {   // surbrillance de la poignée survolée
        const GizmoHandle h = gizmoHandleAt(toWorld(e->position()));
        if (h != m_hoverHandle) {
            m_hoverHandle = h;
            if (h == GizmoHandle::None) updateCursor();
            else setCursor(h == GizmoHandle::X ? Qt::SizeHorCursor : h == GizmoHandle::Y ? Qt::SizeVerCursor : Qt::SizeAllCursor);
        }
    }

    if (m_ctx.isValid(m_dragEntity) && (e->buttons() & Qt::LeftButton)) {
        sf::Vector2f pos = toWorld(e->position()) + m_dragOffset;
        if (e->modifiers() & Qt::ControlModifier) {   // Ctrl : aligner sur la demi-case
            const float step = snapStep();
            pos = { std::round(pos.x / step) * step, std::round(pos.y / step) * step };
        }
        if (m_dragHandle == GizmoHandle::X) pos.y = m_dragStart.y;   // flèche X : l'axe Y ne bouge pas
        if (m_dragHandle == GizmoHandle::Y) pos.x = m_dragStart.x;
        m_ctx.scene().getComponent<Transform2D>(m_dragEntity).setPosition(pos);
        m_ctx.notifyEntityModified(m_dragEntity, this);
    } else if (m_painting && (e->buttons() & Qt::LeftButton)) {
        // hors carte aussi : la ligne est coupée au bord, pas abandonnée
        if (const TileMapData* map = m_ctx.activeMap())
            strokeTo(map->cellAt(toWorld(e->position()) - m_ctx.activeMapOffset()));
    }
    SfmlWidget::mouseMoveEvent(e);
}

void TilemapView::mouseReleaseEvent(QMouseEvent* e) {
    if (m_panning && (e->button() == Qt::MiddleButton || e->button() == Qt::RightButton)) {
        m_panning = false;
        updateCursor();
    }
    if (e->button() == Qt::LeftButton) {
        m_dragEntity = NULL_ENTITY;
        m_dragHandle = GizmoHandle::None;
        m_resize.handle = -1;
        m_hoverHandle = GizmoHandle::None;   // recalculée au prochain mouvement
        updateCursor();
        if (m_painting) { m_strokeEnd = m_lastPainted; m_hasStrokeEnd = true; }
        m_painting = false;
        m_lastPainted = { INT32_MIN, INT32_MIN };
        if (m_interacting) { m_interacting = false; m_ctx.endInteraction(); }
    }
    SfmlWidget::mouseReleaseEvent(e);
}

void TilemapView::leaveEvent(QEvent* e) {
    if (m_hoverValid) {
        m_hoverValid = false;
        emit hoveredTileChanged(-1, -1, QString());
    }
    SfmlWidget::leaveEvent(e);
}

void TilemapView::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
    case Qt::Key_F: fitToMap(); return;
    case Qt::Key_Delete:
        if (m_ctx.hasSelection()) m_ctx.destroyEntity(m_ctx.selected());
        return;
    case Qt::Key_Escape: m_ctx.clearSelection(); return;
    case Qt::Key_Shift: return;   // l'aperçu de ligne se met à jour à la prochaine image
    default: break;
    }
    SfmlWidget::keyPressEvent(e);
}

// ---- Glisser-déposer depuis le panneau Project ----------------------------------------------------
//   .prefab : une instance à l'endroit du dépôt (Ctrl : alignée sur la demi-case)
//   .h      : le comportement est ajouté à l'entité sous la souris

namespace {
QStringList localFiles(const QMimeData* mime) {
    QStringList out;
    if (mime && mime->hasUrls())
        for (const QUrl& url : mime->urls()) if (url.isLocalFile()) out << url.toLocalFile();
    return out;
}
}

void TilemapView::dragEnterEvent(QDragEnterEvent* e) {
    if (!localFiles(e->mimeData()).isEmpty()) e->acceptProposedAction();
}

void TilemapView::dragMoveEvent(QDragMoveEvent* e) {
    if (!localFiles(e->mimeData()).isEmpty()) e->acceptProposedAction();
}

void TilemapView::dropEvent(QDropEvent* e) {
    const QStringList files = localFiles(e->mimeData());
    if (files.isEmpty()) return;
    e->acceptProposedAction();
    sf::Vector2f world = toWorld(e->position());
    if (e->modifiers() & Qt::ControlModifier) {
        const float step = snapStep();
        world = { std::round(world.x / step) * step, std::round(world.y / step) * step };
    }
    m_ctx.dropFiles(files, pickEntity(toWorld(e->position())), world);
    setFocus();
}
