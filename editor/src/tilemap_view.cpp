#include "tilemap_view.h"

#include "components/camera2d.h"
#include "components/collider2d.h"
#include "components/geometry.h"
#include "components/transform2d.h"
#include "systems/render_system.h"
#include "systems/tile_map_render_system.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

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
    if (m_ctx.map().width == 0) {   // carte de démo tant qu'il n'y a pas de projet
        buildTileset();
        buildDemoMap();
    }

    onInit = [this](sf::RenderWindow& target) { init(target); };
    onFrame = [this](sf::RenderWindow& target) { frame(target); };

    connect(&m_ctx, &EditorContext::toolChanged, this, [this] { updateCursor(); });
    updateCursor();
}

TilemapView::~TilemapView() = default;

void TilemapView::buildTileset() {
    // Feuilles dual-grid : 16 colonnes (une par masque de coins) x N lignes de 16 px (voir tilesetFiles())
    TileSet& ts = m_ctx.map().tileset;
    ts.cellSize = 16;
    for (TileType type : paintableTileTypes()) {
        std::vector<std::string> files;
        for (const QString& f : tilesetFiles(type)) files.push_back(f.toStdString());
        ts.variants[(int)type] = files;
    }
    // L'eau est animée : chaque ligne de water.png est une frame
    TileAnimation water;
    water.frameTime = 0.15f;
    water.frameCount = 16;
    ts.animations[(int)TileType::Water] = water;
}

void TilemapView::buildDemoMap() {
    TileMap& map = m_ctx.map();
    map.tileSize = 32;
    map.create(64, 40);   // tout en herbe

    auto disc = [&map](float cx, float cy, float rx, float ry, TileType t) {
        for (int y = 0; y < (int)map.height; ++y)
            for (int x = 0; x < (int)map.width; ++x) {
                const float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
                if (dx * dx + dy * dy <= 1.f) map.setTile(x, y, t);
            }
    };
    disc(18.f, 14.f, 11.f, 8.f, TileType::Sand);
    disc(18.f, 14.f, 8.5f, 5.8f, TileType::Water);
    disc(46.f, 32.f, 6.f, 4.f, TileType::Sand);
    disc(46.f, 32.f, 3.5f, 2.2f, TileType::Water);
    for (int x = 0; x < (int)map.width; ++x) {
        const int y = 27 + (int)std::lround(std::sin(x * 0.18f) * 3.f);
        map.fillRect(x, y, 1, 2, TileType::DirtPath);
    }
    map.fillRect(36, 8, 10, 7, TileType::BrickPath);
    map.fillRect(40, 15, 2, 12, TileType::BrickPath);
    map.fillRect(50, 4, 11, 9, TileType::DonjonFloor);
    map.fillRect(46, 10, 4, 2, TileType::BrickPath);
    map.consumeGrowOffset();
}

void TilemapView::init(sf::RenderWindow& target) {
    m_tileRenderer = std::make_unique<TileMapRenderSystem>(target, m_ctx.map(), m_textures);
    m_entityRenderer = std::make_unique<RenderSystem>(target, m_textures);
    m_clock.restart();
}

void TilemapView::fitToMap() {
    sf::RenderWindow* target = window();
    if (!target) { m_needsFit = true; return; }
    const sf::Vector2u size = target->getSize();
    const TileMap& map = m_ctx.map();
    const float mapW = (float)(map.width * map.tileSize);
    const float mapH = (float)(map.height * map.tileSize);
    if (size.x == 0 || size.y == 0 || mapW <= 0.f || mapH <= 0.f) return;
    m_camera.zoom =
        std::clamp(std::min(size.x / mapW, size.y / mapH) * 0.95f, m_camera.minZoom, m_camera.maxZoom);
    m_camera.position = { mapW * 0.5f, mapH * 0.5f };
    m_needsFit = false;
    m_userCamera = false;
}

void TilemapView::resizeEvent(QResizeEvent* e) {
    SfmlWidget::resizeEvent(e);
    if (!m_userCamera) m_needsFit = true;   // les panneaux se placent au démarrage : on garde la carte cadrée
}

void TilemapView::frame(sf::RenderWindow& target) {
    if (!m_tileRenderer) return;
    const float dt = std::min(m_clock.restart().asSeconds(), 0.05f);
    if (m_needsFit) fitToMap();

    m_ctx.map().update(dt);   // animations (eau)
    target.setView(m_camera.makeView(target.getSize()));
    m_tileRenderer->onUpdate(m_ctx.scene(), dt);
    if (m_showGrid) drawGrid(target);
    m_entityRenderer->onUpdate(m_ctx.scene(), dt);
    drawCameras(target);
    drawSelection(target);
    drawGizmo(target);
    drawBrush(target);
}

void TilemapView::drawGrid(sf::RenderTarget& target) {
    const TileMap& map = m_ctx.map();
    if (m_camera.zoom * map.tileSize < 8.f) return;   // trop dézoomé : grille illisible
    const float ts = (float)map.tileSize;
    const float w = map.width * ts, h = map.height * ts;
    const sf::Color c(0, 0, 0, 45);
    sf::VertexArray lines(sf::PrimitiveType::Lines);
    for (uint32_t x = 0; x <= map.width; ++x) { lines.append({ { x * ts, 0.f }, c }); lines.append({ { x * ts, h }, c }); }
    for (uint32_t y = 0; y <= map.height; ++y) { lines.append({ { 0.f, y * ts }, c }); lines.append({ { w, y * ts }, c }); }
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
    const TileMap& map = m_ctx.map();
    const float ts = (float)map.tileSize;
    const EditorTool tool = m_ctx.tool();
    if (tool == EditorTool::Select) return;

    const int n = (tool == EditorTool::Pick) ? 1 : m_ctx.brushSize();
    const sf::Vector2i start = m_hoveredTile - sf::Vector2i((n - 1) / 2, (n - 1) / 2);
    sf::RectangleShape r({ n * ts, n * ts });
    r.setPosition(map.tileToWorld(start.x, start.y));
    r.setOutlineThickness(-1.5f / m_camera.zoom);
    if (tool == EditorTool::Erase) { r.setFillColor(sf::Color(255, 60, 60, 50)); r.setOutlineColor(sf::Color(255, 90, 90, 200)); }
    else if (tool == EditorTool::Pick) { r.setFillColor(sf::Color::Transparent); r.setOutlineColor(sf::Color(120, 200, 255, 230)); }
    else { r.setFillColor(sf::Color(255, 255, 255, 40)); r.setOutlineColor(sf::Color(255, 255, 255, 200)); }
    target.draw(r);
}

void TilemapView::updateCursor() {
    if (m_panning) return;
    if (m_ctx.tool() == EditorTool::Select) unsetCursor();
    else setCursor(Qt::CrossCursor);
}

sf::Vector2f TilemapView::toWorld(const QPointF& p) {
    const qreal dpr = devicePixelRatioF();
    const sf::Vector2u size = window() ? window()->getSize() : sf::Vector2u(1, 1);
    const sf::Vector2f half = { size.x * 0.5f, size.y * 0.5f };
    const sf::Vector2f px = { float(p.x() * dpr), float(p.y() * dpr) };
    return m_camera.position + (px - half) / m_camera.zoom;
}

Entity TilemapView::pickEntity(sf::Vector2f world) {
    Registry& reg = m_ctx.scene();
    Entity best = NULL_ENTITY;
    int bestLayer = INT32_MIN;
    for (Entity e : m_ctx.entities()) {
        if (!reg.hasComponent<Transform2D>(e)) continue;
        const sf::Vector2f local = reg.getComponent<Transform2D>(e).getLocalMatrix().getInverse().transformPoint(world);
        if (!localBounds(reg, e).contains(local)) continue;
        const int layer = reg.hasComponent<Geometry>(e) ? reg.getComponent<Geometry>(e).layer : 0;
        if (layer >= bestLayer) { best = e; bestLayer = layer; }   // à égalité : la plus récente (dessinée au-dessus)
    }
    return best;
}

void TilemapView::paintAt(sf::Vector2i tile) {
    TileMap& map = m_ctx.map();
    const EditorTool tool = m_ctx.tool();

    if (tool == EditorTool::Pick) {
        const TileType t = map.getType(tile.x, tile.y);
        const auto paintable = paintableTileTypes();
        if (std::find(paintable.begin(), paintable.end(), t) != paintable.end()) {
            m_ctx.setPaintType(t);
            m_ctx.setTool(EditorTool::Paint);
        }
        return;
    }

    const TileType type = (tool == EditorTool::Erase) ? TileType::None : m_ctx.paintType();
    const int n = m_ctx.brushSize();
    const sf::Vector2i start = tile - sf::Vector2i((n - 1) / 2, (n - 1) / 2);
    bool changed = false;
    for (int y = start.y; y < start.y + n; ++y)
        for (int x = start.x; x < start.x + n; ++x) {
            // Pour l'instant la carte s'agrandit vers la droite et le bas seulement
            // (vers x<0 / y<0 elle décalerait toutes les tiles sous les entités).
            if (x < 0 || y < 0) continue;
            const bool inside = x < (int)map.width && y < (int)map.height;
            if (!inside && type == TileType::None) continue;            // gommer hors carte : rien à faire
            if (inside && map.getType(x, y) == type) continue;
            map.setTile(x, y, type);
            changed = true;
        }
    if (changed) map.consumeGrowOffset();
}

void TilemapView::updateHover(const QPointF& pos) {
    const TileMap& map = m_ctx.map();
    const sf::Vector2i tile = map.worldToTile(toWorld(pos));
    const bool valid = tile.x >= 0 && tile.y >= 0;
    if (valid == m_hoverValid && tile == m_hoveredTile) return;
    m_hoveredTile = tile;
    m_hoverValid = valid;
    const bool inside = valid && tile.x < (int)map.width && tile.y < (int)map.height;
    emit hoveredTileChanged(inside ? tile.x : -1, inside ? tile.y : -1,
                            inside ? tileTypeName(map.getType(tile.x, tile.y)) : QString());
}

void TilemapView::wheelEvent(QWheelEvent* e) {
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
        } else {
            updateHover(e->position());
            if (m_hoverValid) {
                paintAt(m_hoveredTile);
                m_lastPainted = m_hoveredTile;
                m_painting = m_ctx.tool() != EditorTool::Pick;
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
            const float step = m_ctx.map().tileSize * 0.5f;
            pos = { std::round(pos.x / step) * step, std::round(pos.y / step) * step };
        }
        if (m_dragHandle == GizmoHandle::X) pos.y = m_dragStart.y;   // flèche X : l'axe Y ne bouge pas
        if (m_dragHandle == GizmoHandle::Y) pos.x = m_dragStart.x;
        m_ctx.scene().getComponent<Transform2D>(m_dragEntity).setPosition(pos);
        m_ctx.notifyEntityModified(m_dragEntity, this);
    } else if (m_painting && m_hoverValid && m_hoveredTile != m_lastPainted) {
        paintAt(m_hoveredTile);
        m_lastPainted = m_hoveredTile;
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
        m_hoverHandle = GizmoHandle::None;   // recalculée au prochain mouvement
        updateCursor();
        m_painting = false;
        m_lastPainted = { INT32_MIN, INT32_MIN };
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
    default: break;
    }
    SfmlWidget::keyPressEvent(e);
}
