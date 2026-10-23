#pragma once
#include "ecs/system.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include "components/unitComp.h"
#include "components/selectable.h"
#include "components/buildingComp.h"
#include "components/rtsPlayerState.h"
#include "../utils/unit_formation.h"
#include "RtsInput_state.h"
#include <SFML/Graphics.hpp>
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>
#include <utility>

class SelectionSystem : public ISystem {
public:
    SelectionSystem(sf::RenderWindow& window, InputState& input)
        : m_window(window), m_input(input) {
        priority = -2;
    }

    void onUpdate(Registry& registry, float dt) override {
        m_sinceLastClick += dt;

        auto toWorld = [&](sf::Vector2f screen)
            {
                return m_window.mapPixelToCoords(
                    { int(screen.x), int(screen.y) });
            };

        if (m_input.saveFormationSlotPressed >= 0)
            saveSelectedFormation(registry, m_input.saveFormationSlotPressed);
        if (m_input.applyFormationSlotPressed >= 0)
            armSavedFormation(registry, m_input.applyFormationSlotPressed);
        if (m_input.chargePressed)
            issueChargeOrder(registry);

        if (m_input.leftMousePressed) {
            m_input.dragStart = m_input.mouseScreenPos;
            m_input.isDragging = true;
        }

        if (m_input.leftMouseReleased && m_input.isDragging) {
            m_input.isDragging = false;

            if (m_input.uiConsumedClick)
                return;

            sf::Vector2f a = toWorld(m_input.dragStart);
            sf::Vector2f b = toWorld(m_input.mouseScreenPos);
            float dx = b.x - a.x;
            float dy = b.y - a.y;

            if (std::abs(dx) < 6.f && std::abs(dy) < 6.f) {
                clickSelect(registry, b);
            }
            else {
                sf::FloatRect rect(
                    sf::Vector2f(std::min(a.x, b.x), std::min(a.y, b.y)),
                    sf::Vector2f(std::abs(dx), std::abs(dy)));
                boxSelect(registry, rect);
            }
        }

        if (m_input.rightMousePressed)
            beginRightOrder(registry, toWorld(m_input.mouseScreenPos));

        if (m_rightDragActive) {
            const sf::Vector2f dragEnd = toWorld(m_input.mouseScreenPos);
            updateFormationPreview(registry, dragEnd);
            if (m_input.rightMouseReleased) {
                issueMoveOrder(registry, m_rightDragUnits, m_rightDragStart, dragEnd);
                updateFormationPreview(registry, dragEnd, false);
                m_rightDragActive = false;
                m_rightDragUnits.clear();
            }
        }
    }

private:
    sf::RenderWindow& m_window;
    InputState& m_input;
    sf::Vector2f m_rightDragStart{ 0.f, 0.f };
    std::vector<Entity> m_rightDragUnits;
    bool m_rightDragActive = false;
    uint32_t m_nextMovementGroup = 1;

    // double clic : meme unite cliquee deux fois en moins de kDoubleClickTime
    static constexpr float kDoubleClickTime = 0.35f;
    Entity m_lastClickedEntity = NULL_ENTITY;
    float m_sinceLastClick = 999.f;

    bool isControlPressed() const {
        return sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl)
            || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl)
            || m_input.gamepadAdditiveSelection;
    }

    void beginRightOrder(Registry& registry, sf::Vector2f world) {
        Entity building = pickConstructionSite(registry, world);
        if (building.index != UINT32_MAX) {
            issueConstructionOrders(registry, building);
            return;
        }

        Entity enemy = pickEnemy(registry, world);
        if (enemy.index != UINT32_MAX) {
            issueAttackOrder(registry, enemy);
            return;
        }

        m_rightDragUnits.clear();
        registry.view<ECS::Unit, ECS::Selectable>([&](Entity entity, ECS::Unit& unit,
            ECS::Selectable& selectable) {
            if (unit.team == ECS::Unit::Team::Player && selectable.playerOwned && selectable.selected)
                m_rightDragUnits.push_back(entity);
        });
        std::sort(m_rightDragUnits.begin(), m_rightDragUnits.end(),
            [](Entity a, Entity b) { return a.index < b.index; });
        if (m_rightDragUnits.empty()) return;

        m_rightDragStart = world;
        m_rightDragActive = true;
        updateFormationPreview(registry, world);
    }

    void updateFormationPreview(Registry& registry, sf::Vector2f dragEnd, bool active = true) {
        registry.view<ECS::RtsHudState>([&](Entity, ECS::RtsHudState& hud) {
            hud.formationPreviewActive = active;
            hud.formationPreviewCenter = m_rightDragStart;
            hud.formationPreviewEnd = dragEnd;
            hud.formationPreviewUnitCount = active
                ? static_cast<uint32_t>(m_rightDragUnits.size()) : 0;
            hud.formationPreviewPositions.clear();
            if (!active) return;

            const bool simpleClick = std::abs(dragEnd.x - m_rightDragStart.x) < 12.f
                && std::abs(dragEnd.y - m_rightDragStart.y) < 12.f;
            const bool shortcutMatches = hud.armedFormationShortcut >= 0
                && hud.savedFormationOffsets[hud.armedFormationShortcut].size() == m_rightDragUnits.size();
            if (simpleClick && shortcutMatches) {
                for (const sf::Vector2f offset : hud.savedFormationOffsets[hud.armedFormationShortcut])
                    hud.formationPreviewPositions.push_back(m_rightDragStart + offset);
            }
            else if (simpleClick) {
                sf::Vector2f currentCenter{ 0.f, 0.f };
                std::vector<sf::Vector2f> currentPositions;
                currentPositions.reserve(m_rightDragUnits.size());
                for (const Entity unit : m_rightDragUnits) {
                    if (!registry.isValid(unit) || !registry.hasComponent<Transform2D>(unit)) continue;
                    const sf::Vector2f position = registry.getComponent<Transform2D>(unit).getPosition();
                    currentPositions.push_back(position);
                    currentCenter += position;
                }
                if (!currentPositions.empty()) {
                    currentCenter /= static_cast<float>(currentPositions.size());
                    for (const sf::Vector2f position : currentPositions)
                        hud.formationPreviewPositions.push_back(m_rightDragStart + (position - currentCenter));
                }
            }
            if (hud.formationPreviewPositions.size() != m_rightDragUnits.size())
                hud.formationPreviewPositions = Rts::formationPositions(m_rightDragStart, dragEnd,
                    m_rightDragUnits.size());
        });
    }

    void clearSelection(Registry& registry) {
        registry.view<ECS::Selectable>([&](Entity, ECS::Selectable& selectable) {
            selectable.selected = false;
            });
    }

    void clickSelect(Registry& registry, sf::Vector2f pos) {
        Entity picked = NULL_ENTITY;

        registry.view<ECS::Selectable, Transform2D>(
            [&](Entity e, ECS::Selectable& selectable, Transform2D& t) {
                if (!selectable.playerOwned) return;
                if (picked.index != UINT32_MAX) return;
                if (registry.hasComponent<ECS::Building>(e)) return; // les batiments : passe suivante
                sf::Vector2f p = t.getPosition();
                if (std::hypot(p.x - pos.x, p.y - pos.y) < 28.f)
                    picked = e;
            });

        // batiments : on clique n'importe ou sur leur taille (celle du png), les unites passent devant
        if (picked.index == UINT32_MAX) {
            registry.view<ECS::Selectable, ECS::Building, Transform2D>(
                [&](Entity e, ECS::Selectable& selectable, ECS::Building& building, Transform2D& t) {
                    if (!selectable.playerOwned || picked.index != UINT32_MAX || building.pv <= 0) return;
                    const sf::Vector2f p = t.getPosition();
                    const sf::Vector2f scale = t.getScale();
                    if (std::abs(pos.x - p.x) <= building.halfSize.x * scale.x
                        && std::abs(pos.y - p.y) <= building.halfSize.y * scale.y)
                        picked = e;
                });
        }

        const bool additive = isControlPressed();
        if (!additive) clearSelection(registry);

        if (picked.index == UINT32_MAX) {
            m_lastClickedEntity = NULL_ENTITY;
            return;
        }

        registry.getComponent<ECS::Selectable>(picked).selected = true;

        const bool doubleClick = picked == m_lastClickedEntity && m_sinceLastClick <= kDoubleClickTime;
        m_sinceLastClick = 0.f;
        if (doubleClick && registry.hasComponent<ECS::Unit>(picked)) {
            selectAllOfType(registry, registry.getComponent<ECS::Unit>(picked).type);
            m_lastClickedEntity = NULL_ENTITY; // un 3e clic repart de zero
            return;
        }
        m_lastClickedEntity = picked;

        if (additive)
            setSelectionFeedback(registry, "Unite ajoutee a la selection.");
    }

    // double clic : selectionne toutes les unites du joueur du meme type (toute la carte)
    void selectAllOfType(Registry& registry, ECS::Unit::Type type) {
        int count = 0;
        registry.view<ECS::Unit, ECS::Selectable>([&](Entity, ECS::Unit& unit, ECS::Selectable& selectable) {
            if (unit.team != ECS::Unit::Team::Player || !selectable.playerOwned
                || unit.pv <= 0 || unit.type != type) return;
            selectable.selected = true;
            ++count;
        });
        setSelectionFeedback(registry, std::to_string(count) + " " + pluralName(type) + " selectionnes.");
    }

    static const char* pluralName(ECS::Unit::Type type) {
        switch (type) {
        case ECS::Unit::Type::Archer: return "archers";
        case ECS::Unit::Type::Builder: return "batisseurs";
        case ECS::Unit::Type::Healer: return "soigneurs";
        case ECS::Unit::Type::Melee: return "guerriers";
        case ECS::Unit::Type::Soldier: return "soldats";
        }
        return "unites";
    }

    void boxSelect(Registry& registry, const sf::FloatRect& rect) {
        const bool additive = isControlPressed();
        if (!additive) clearSelection(registry);

        registry.view<ECS::Selectable, Transform2D>(
            [&](Entity, ECS::Selectable& selectable, Transform2D& t) {
                if (!selectable.playerOwned) return;
                if (rect.contains(t.getPosition()))
                    selectable.selected = true;
            });
        if (additive) setSelectionFeedback(registry, "Unites ajoutees a la selection.");
    }

    void setSelectionFeedback(Registry& registry, const std::string& message) {
        registry.view<ECS::RtsHudState>([&](Entity, ECS::RtsHudState& hud) {
            hud.message = message;
            hud.messageTimer = 1.5f;
        });
    }

    void saveSelectedFormation(Registry& registry, int slot) {
        std::vector<std::pair<Entity, sf::Vector2f>> selected;
        registry.view<ECS::Unit, ECS::Selectable, Transform2D>(
            [&](Entity entity, ECS::Unit& unit, ECS::Selectable& selectable, Transform2D& transform) {
                if (unit.team == ECS::Unit::Team::Player && selectable.playerOwned && selectable.selected)
                    selected.push_back({ entity, transform.getPosition() });
            });
        std::sort(selected.begin(), selected.end(),
            [](const auto& a, const auto& b) { return a.first.index < b.first.index; });
        if (selected.empty()) {
            setSelectionFeedback(registry, "Selectionne des troupes avant Shift+&/1, Shift+é/2, Shift+\"/3 ou Shift+'/4.");
            return;
        }

        sf::Vector2f center{ 0.f, 0.f };
        for (const auto& unit : selected) center += unit.second;
        center /= static_cast<float>(selected.size());
        registry.view<ECS::RtsHudState>([&](Entity, ECS::RtsHudState& hud) {
            auto& offsets = hud.savedFormationOffsets[slot];
            auto& savedUnits = hud.savedFormationUnits[slot];
            offsets.clear();
            offsets.reserve(selected.size());
            savedUnits.clear();
            savedUnits.reserve(selected.size());
            for (const auto& unit : selected) {
                offsets.push_back(unit.second - center);
                savedUnits.push_back(unit.first);
            }
            hud.armedFormationShortcut = -1;
            hud.message = "Formation memorisee sur " + formationKeyLabel(slot) + ".";
            hud.messageTimer = 1.8f;
        });
    }

    void armSavedFormation(Registry& registry, int slot) {
        registry.view<ECS::RtsHudState>([&](Entity, ECS::RtsHudState& hud) {
            if (hud.savedFormationOffsets[slot].empty()) {
                hud.message = "Memorise une formation avec Shift+" + formationKeyLabel(slot) + ".";
                hud.messageTimer = 1.8f;
                return;
            }

            clearSelection(registry);
            std::size_t selectedCount = 0;
            for (const Entity entity : hud.savedFormationUnits[slot]) {
                if (!registry.isValid(entity) || !registry.hasComponent<ECS::Unit>(entity)
                    || !registry.hasComponent<ECS::Selectable>(entity))
                    continue;
                ECS::Unit& unit = registry.getComponent<ECS::Unit>(entity);
                ECS::Selectable& selectable = registry.getComponent<ECS::Selectable>(entity);
                if (unit.team != ECS::Unit::Team::Player || !selectable.playerOwned) continue;
                selectable.selected = true;
                ++selectedCount;
            }
            if (selectedCount == 0) {
                hud.armedFormationShortcut = -1;
                hud.message = "Aucune troupe disponible dans le groupe "
                    + formationKeyLabel(slot) + ".";
                hud.messageTimer = 1.8f;
                return;
            }
            hud.armedFormationShortcut = slot;
            hud.message = "Groupe " + formationKeyLabel(slot) + " selectionne.";
            hud.messageTimer = 1.8f;
        });
    }

    Entity pickEnemy(Registry& registry, sf::Vector2f pos) {
        Entity picked = NULL_ENTITY;
        registry.view<ECS::Unit, Transform2D>(
            [&](Entity e, ECS::Unit& u, Transform2D& t) {
                if (u.team != ECS::Unit::Team::Enemy) return;
                if (!u.visibleToPlayer) return;
                if (picked.index != UINT32_MAX)       return;
                sf::Vector2f p = t.getPosition();
                if (std::hypot(p.x - pos.x, p.y - pos.y) < 28.f)
                    picked = e;
            });
        return picked;
    }

    Entity pickConstructionSite(Registry& registry, sf::Vector2f pos) {
        Entity picked = NULL_ENTITY;
        registry.view<ECS::Building, Transform2D>(
            [&](Entity e, ECS::Building& building, Transform2D& transform) {
                if (building.constructionProgress >= 1.f || building.type == ECS::Building::Type::Headquarters)
                    return;
                const sf::Vector2f p = transform.getPosition();
                // taille finale du batiment (+ une marge), 62 au minimum comme avant
                const bool inside = std::abs(pos.x - p.x) <= std::max(62.f, building.halfSize.x + 6.f)
                    && std::abs(pos.y - p.y) <= std::max(62.f, building.halfSize.y + 6.f);
                if (picked.index == UINT32_MAX && inside)
                    picked = e;
            });
        return picked;
    }

    void issueConstructionOrders(Registry& registry, Entity building) {
        registry.view<ECS::RtsCommandQueue>([&](Entity, ECS::RtsCommandQueue& queue) {
            registry.view<ECS::Unit, ECS::Selectable>([&](Entity builderEntity, ECS::Unit& unit, ECS::Selectable& selectable) {
                if (unit.type != ECS::Unit::Type::Builder || unit.team != ECS::Unit::Team::Player
                    || !selectable.selected || !selectable.playerOwned) return;
                queue.pending.push_back({ ECS::RtsCommandQueue::Type::AssignBuilder, building, {}, builderEntity });
            });
        });
    }

    void issueAttackOrder(Registry& registry, Entity target) {
        registry.view<ECS::Unit, ECS::Selectable>([&](Entity, ECS::Unit& u, ECS::Selectable& selectable) {
            if (u.team != ECS::Unit::Team::Player || !selectable.selected) return;
            u.chargeOrder = false;
            u.target = target;
            u.hasDestination = false;
            u.constructionTarget = NULL_ENTITY;
            });
    }

    void issueChargeOrder(Registry& registry) {
        std::vector<Entity> selectedUnits;
        registry.view<ECS::Unit, ECS::Selectable>(
            [&](Entity entity, ECS::Unit& unit, ECS::Selectable& selectable) {
                if (unit.team != ECS::Unit::Team::Player || !selectable.playerOwned || !selectable.selected)
                    return;
                selectedUnits.push_back(entity);
            });
        if (selectedUnits.empty()) {
            setSelectionFeedback(registry, "Selectionne des troupes avant de charger.");
            return;
        }
        bool enemiesExist = false;
        registry.view<ECS::Unit>([&](Entity, ECS::Unit& unit) {
            if (unit.team == ECS::Unit::Team::Enemy && unit.pv > 0) enemiesExist = true;
        });
        if (!enemiesExist) {
            setSelectionFeedback(registry, "Aucun ennemi a charger.");
            return;
        }
        for (const Entity entity : selectedUnits) {
            ECS::Unit& unit = registry.getComponent<ECS::Unit>(entity);
            unit.chargeOrder = true;
            unit.target = NULL_ENTITY;
            unit.hasDestination = false;
            unit.movementGroup = 0;
            unit.constructionTarget = NULL_ENTITY;
        }
        setSelectionFeedback(registry, "Charge ! Les troupes poursuivent les ennemis restants.");
    }

    void issueMoveOrder(Registry& registry, const std::vector<Entity>& units,
        sf::Vector2f center, sf::Vector2f dragEnd) {
        std::vector<Entity> activeUnits;
        activeUnits.reserve(units.size());
        for (const Entity entity : units) {
            if (registry.isValid(entity) && registry.hasComponent<ECS::Unit>(entity)
                && registry.hasComponent<Transform2D>(entity))
                activeUnits.push_back(entity);
        }
        if (activeUnits.empty()) return;

        std::vector<sf::Vector2f> positions;
        bool usedShortcut = false;
        registry.view<ECS::RtsHudState>([&](Entity, ECS::RtsHudState& hud) {
            if (hud.armedFormationShortcut < 0) return;
            const auto& offsets = hud.savedFormationOffsets[hud.armedFormationShortcut];
            if (offsets.size() == activeUnits.size()) {
                positions.reserve(activeUnits.size());
                for (sf::Vector2f offset : offsets)
                    positions.push_back(center + offset);
                usedShortcut = true;
            }
            else {
                hud.message = "La formation " + formationKeyLabel(hud.armedFormationShortcut)
                    + " ne correspond pas a ce nombre de troupes.";
                hud.messageTimer = 1.8f;
            }
            hud.armedFormationShortcut = -1;
        });

        const bool simpleClick = std::abs(dragEnd.x - center.x) < 12.f
            && std::abs(dragEnd.y - center.y) < 12.f;
        if (!usedShortcut && simpleClick) {
            sf::Vector2f currentCenter{ 0.f, 0.f };
            std::vector<sf::Vector2f> currentPositions;
            currentPositions.reserve(activeUnits.size());
            for (const Entity entity : activeUnits) {
                const sf::Vector2f position = registry.getComponent<Transform2D>(entity).getPosition();
                currentPositions.push_back(position);
                currentCenter += position;
            }
            if (!currentPositions.empty()) {
                currentCenter /= static_cast<float>(currentPositions.size());
                for (const sf::Vector2f position : currentPositions)
                    positions.push_back(center + (position - currentCenter));
            }
        }
        if (positions.size() != activeUnits.size())
            positions = Rts::formationPositions(center, dragEnd, activeUnits.size());
        uint32_t movementGroup = m_nextMovementGroup++;
        if (movementGroup == 0) movementGroup = m_nextMovementGroup++;
        for (std::size_t i = 0; i < activeUnits.size(); ++i) {
            ECS::Unit& unit = registry.getComponent<ECS::Unit>(activeUnits[i]);
            unit.chargeOrder = false;
            unit.movementGroup = movementGroup;
            unit.target = NULL_ENTITY;
            unit.constructionTarget = NULL_ENTITY;
            unit.destination = positions[i];
            unit.hasDestination = true;
        }
    }

    static std::string formationKeyLabel(int slot) {
        static const char* labels[] = { "& / 1", "é / 2", "\" / 3", "' / 4" };
        return labels[slot];
    }
};
