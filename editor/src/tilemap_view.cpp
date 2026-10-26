#include "tilemap_view.h"
#include "systems/tile_map_render_system.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {
const char* typeName(TileType t) {
    switch (t) {
    case TileType::Sand:        return "Sable";
    case TileType::Water:       return "Eau";
    case TileType::Grass:       return "Herbe";
    case TileType::BrickPath:   return "Chemin de briques";
    case TileType::DirtPath:    return "Chemin de terre";
    case TileType::DonjonFloor: return "Sol de donjon";
    case TileType::Wall:        return "Mur";
    case TileType::Path:        return "Chemin";
    default:                    return "Vide";
    }
}
}

TilemapView::TilemapView(QWidget* parent) : SfmlWidget(parent) {
    setMouseTracking(true);
    buildTileset();
    buildDemoMap();

    m_camera.minZoom = 0.1f;
    m_camera.maxZoom = 8.f;

    onInit = [this](sf::RenderWindow& target) { init(target); };
    onFrame = [this](sf::RenderWindow& target) { frame(target); };
}

TilemapView::~TilemapView() = default;

void TilemapView::buildTileset() {
    // Feuilles dual-grid : 16 colonnes (une par masque de coins) x N lignes de 16 px.
    // Copiées depuis projects/sandbox/assets/tilesets vers bin/assets/tilesets au build.
    const QString dir = QCoreApplication::applicationDirPath() + "/assets/tilesets/";
    auto file = [&dir](const char* name) { return QDir::toNativeSeparators(dir + name).toStdString(); };

    TileSet& ts = m_map.tileset;
    ts.cellSize = 16;
    // Plusieurs feuilles pour un type : les cases pleines piochent au hasard entre elles,
    // les transitions utilisent la première. Répéter une feuille augmente sa fréquence
    // (ici : herbe unie 3 fois sur 4, fleurs 1 fois sur 4).
    ts.variants[(int)TileType::Grass]       = { file("grass.png"), file("grass.png"), file("grass.png"),
                                                file("grass_flowers.png") };
    ts.variants[(int)TileType::Sand]        = { file("sand.png") };
    ts.variants[(int)TileType::Water]       = { file("water.png") };
    ts.variants[(int)TileType::DirtPath]    = { file("dirt_path.png") };
    ts.variants[(int)TileType::BrickPath]   = { file("stone_path.png") };
    ts.variants[(int)TileType::DonjonFloor] = { file("dungeon_floor.png") };

    // L'eau est animée : chaque ligne de water.png est une frame
    TileAnimation water;
    water.frameTime = 0.15f;
    water.frameCount = 16;
    ts.animations[(int)TileType::Water] = water;
}

void TilemapView::buildDemoMap() {
    m_map.tileSize = 32;
    m_map.create(64, 40);                 // tout en herbe

    // Lac entouré de sable
    auto disc = [this](float cx, float cy, float rx, float ry, TileType t) {
        for (int y = 0; y < (int)m_map.height; ++y)
            for (int x = 0; x < (int)m_map.width; ++x) {
                const float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
                if (dx * dx + dy * dy <= 1.f) m_map.setTile(x, y, t);
            }
    };
    disc(18.f, 14.f, 11.f, 8.f, TileType::Sand);
    disc(18.f, 14.f, 8.5f, 5.8f, TileType::Water);
    disc(46.f, 32.f, 6.f, 4.f, TileType::Sand);
    disc(46.f, 32.f, 3.5f, 2.2f, TileType::Water);

    // Chemin de terre qui serpente
    for (int x = 0; x < (int)m_map.width; ++x) {
        const int y = 27 + (int)std::lround(std::sin(x * 0.18f) * 3.f);
        m_map.fillRect(x, y, 1, 2, TileType::DirtPath);
    }

    // Place pavée et chemin de briques vers le donjon
    m_map.fillRect(36, 8, 10, 7, TileType::BrickPath);
    m_map.fillRect(40, 15, 2, 12, TileType::BrickPath);

    // Salle de donjon
    m_map.fillRect(50, 4, 11, 9, TileType::DonjonFloor);
    m_map.fillRect(46, 10, 4, 2, TileType::BrickPath);

    m_map.consumeGrowOffset();
}

void TilemapView::init(sf::RenderWindow& target) {
    m_renderer = std::make_unique<TileMapRenderSystem>(target, m_map, m_camera, m_textures);
    m_clock.restart();
}

void TilemapView::fitToMap() {
    sf::RenderWindow* target = window();
    if (!target) { m_needsFit = true; return; }
    const sf::Vector2u size = target->getSize();
    const float mapW = (float)(m_map.width * m_map.tileSize);
    const float mapH = (float)(m_map.height * m_map.tileSize);
    if (size.x == 0 || size.y == 0 || mapW <= 0.f || mapH <= 0.f) return;
    m_camera.zoom = m_camera.targetZoom =
        std::clamp(std::min(size.x / mapW, size.y / mapH) * 0.95f, m_camera.minZoom, m_camera.maxZoom);
    m_camera.position = { mapW * 0.5f, mapH * 0.5f };
    m_needsFit = false;
}

void TilemapView::frame(sf::RenderWindow& target) {
    if (!m_renderer) return;
    const float dt = std::min(m_clock.restart().asSeconds(), 0.05f);
    if (m_needsFit) fitToMap();

    m_map.update(dt);   // animations (eau)
    target.setView(m_camera.makeView(target.getSize()));
    m_renderer->onUpdate(m_registry, dt);
    if (m_showGrid) drawGrid(target);

    // Case survolée
    if (m_hovered.x >= 0) {
        const float ts = (float)m_map.tileSize;
        sf::RectangleShape r({ ts, ts });
        r.setPosition(m_map.tileToWorld(m_hovered.x, m_hovered.y));
        r.setFillColor(sf::Color(255, 255, 255, 30));
        r.setOutlineColor(sf::Color(255, 255, 255, 160));
        r.setOutlineThickness(-1.f / m_camera.zoom);
        target.draw(r);
    }
}

void TilemapView::drawGrid(sf::RenderTarget& target) {
    if (m_camera.zoom * m_map.tileSize < 8.f) return;   // trop dézoomé : grille illisible
    const float ts = (float)m_map.tileSize;
    const float w = m_map.width * ts, h = m_map.height * ts;
    const sf::Color c(0, 0, 0, 45);
    sf::VertexArray lines(sf::PrimitiveType::Lines);
    for (uint32_t x = 0; x <= m_map.width; ++x) {
        lines.append({ { x * ts, 0.f }, c });
        lines.append({ { x * ts, h }, c });
    }
    for (uint32_t y = 0; y <= m_map.height; ++y) {
        lines.append({ { 0.f, y * ts }, c });
        lines.append({ { w, y * ts }, c });
    }
    target.draw(lines);
}

sf::Vector2f TilemapView::toWorld(const QPointF& p) {
    const qreal dpr = devicePixelRatioF();
    const sf::Vector2u size = window() ? window()->getSize() : sf::Vector2u(1, 1);
    const sf::Vector2f half = { size.x * 0.5f, size.y * 0.5f };
    const sf::Vector2f px = { float(p.x() * dpr), float(p.y() * dpr) };
    return m_camera.position + (px - half) / m_camera.zoom;
}

void TilemapView::wheelEvent(QWheelEvent* e) {
    const float steps = e->angleDelta().y() / 120.f;
    if (steps == 0.f) return;
    const sf::Vector2f before = toWorld(e->position());
    m_camera.zoom = std::clamp(m_camera.zoom * std::pow(1.15f, steps), m_camera.minZoom, m_camera.maxZoom);
    m_camera.targetZoom = m_camera.zoom;
    const sf::Vector2f after = toWorld(e->position());
    m_camera.position += before - after;   // le point sous la souris ne bouge pas
    e->accept();
}

void TilemapView::mousePressEvent(QMouseEvent* e) {
    setFocus();
    if (e->button() == Qt::MiddleButton || e->button() == Qt::RightButton) {
        m_panning = true;
        m_lastMouse = e->position();
        setCursor(Qt::ClosedHandCursor);
    }
    SfmlWidget::mousePressEvent(e);
}

void TilemapView::mouseMoveEvent(QMouseEvent* e) {
    if (m_panning) {
        const QPointF d = (e->position() - m_lastMouse) * devicePixelRatioF();
        m_camera.position -= sf::Vector2f(float(d.x()), float(d.y())) / m_camera.zoom;
        m_lastMouse = e->position();
    }

    const sf::Vector2i tile = m_map.worldToTile(toWorld(e->position()));
    const bool inside = tile.x >= 0 && tile.y >= 0 && tile.x < (int)m_map.width && tile.y < (int)m_map.height;
    const sf::Vector2i hovered = inside ? tile : sf::Vector2i(-1, -1);
    if (hovered != m_hovered) {
        m_hovered = hovered;
        emit hoveredTileChanged(hovered.x, hovered.y,
                                inside ? QString::fromUtf8(typeName(m_map.getType(tile.x, tile.y))) : QString());
    }
    SfmlWidget::mouseMoveEvent(e);
}

void TilemapView::mouseReleaseEvent(QMouseEvent* e) {
    if (m_panning && (e->button() == Qt::MiddleButton || e->button() == Qt::RightButton)) {
        m_panning = false;
        unsetCursor();
    }
    SfmlWidget::mouseReleaseEvent(e);
}

void TilemapView::leaveEvent(QEvent* e) {
    if (m_hovered.x >= 0) {
        m_hovered = { -1, -1 };
        emit hoveredTileChanged(-1, -1, QString());
    }
    SfmlWidget::leaveEvent(e);
}

void TilemapView::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_F) { fitToMap(); return; }
    SfmlWidget::keyPressEvent(e);
}
