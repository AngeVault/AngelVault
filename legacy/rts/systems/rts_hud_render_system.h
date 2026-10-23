#pragma once

#include "../components/buildingComp.h"
#include "../components/enemyWaveComp.h"
#include "../components/rtsPlayerState.h"
#include "../components/selectable.h"
#include "../components/unitComp.h"
#include "../RtsInput_state.h"
#include "../utils/rts_ui_draw.h"
#include <components/transform2d.h>
#include <SFML/Graphics.hpp>
#include <ecs/registry.h>
#include <ecs/system.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

class RtsHudRenderSystem : public ISystem {
public:
    RtsHudRenderSystem(sf::RenderWindow& window, const InputState& input) : m_window(window), m_input(input) {
        priority = 90;
        m_hasFont = m_font.openFromFile("games/rts/assets/fonts/font.ttf");
    }

    void onUpdate(Registry& registry, float) override {
        ECS::RtsHudState* hud = nullptr;
        ECS::PlayerEconomy* economy = nullptr;
        ECS::RtsScore* score = nullptr;
        ECS::EnemyWaveDirector* waves = nullptr;
        registry.view<ECS::RtsHudState>([&](Entity, ECS::RtsHudState& value) { hud = &value; });
        registry.view<ECS::PlayerEconomy>([&](Entity, ECS::PlayerEconomy& value) { economy = &value; });
        registry.view<ECS::RtsScore>([&](Entity, ECS::RtsScore& value) { score = &value; });
        registry.view<ECS::EnemyWaveDirector>([&](Entity, ECS::EnemyWaveDirector& value) { waves = &value; });
        if (!hud || !economy) return;

        refreshButtons(registry, *hud, m_window.getSize());
        const sf::View previousView = m_window.getView();
        const sf::Vector2u size = m_window.getSize();
        const Entity selected = selectedPlayer(registry);
        sf::Vector2i selectedPixel{ 0, 0 };
        const bool selectedBuilding = selected.index != UINT32_MAX
            && registry.hasComponent<ECS::Building>(selected);
        const ECS::Building* selectedBuildingData = selectedBuilding
            ? &registry.getComponent<ECS::Building>(selected) : nullptr;
        const bool showFarmIncome = selectedBuildingData
            && selectedBuildingData->type == ECS::Building::Type::Farm;
        const bool showConstructionStatus = selectedBuildingData
            && selectedBuildingData->constructionProgress < 1.f;
        const bool showRecruitment = selectedBuildingData
            && !selectedBuildingData->recruitmentQueue.empty();
        if ((showFarmIncome || showConstructionStatus || showRecruitment)
            && registry.hasComponent<Transform2D>(selected))
            selectedPixel = m_window.mapCoordsToPixel(registry.getComponent<Transform2D>(selected).getPosition());

        m_window.setView(sf::View(sf::FloatRect({ 0.f, 0.f }, { static_cast<float>(size.x), static_cast<float>(size.y) })));

        RtsUI::drawRoundedRect(m_window, sf::FloatRect({ 12.f, 12.f }, { 112.f, 32.f }),
            sf::Color(255, 228, 240, 245), sf::Color(235, 151, 188), 1.5f, 14.f);
        drawText("Or  " + std::to_string(economy->gold), { 25.f, 18.f }, 15,
            sf::Color(119, 55, 83));
        if (score) {
            RtsUI::drawRoundedRect(m_window, sf::FloatRect({ 244.f, 12.f }, { 112.f, 32.f }),
                sf::Color(255, 228, 240, 245), sf::Color(235, 151, 188), 1.5f, 14.f);
            drawText("Score  " + std::to_string(score->value), { 257.f, 18.f }, 15,
                sf::Color(119, 55, 83));
        }
        if (waves) {
            const unsigned int shownWave = waves->wave == 0 ? 1 : waves->wave;
            RtsUI::drawRoundedRect(m_window, sf::FloatRect({ 132.f, 12.f }, { 104.f, 32.f }),
                sf::Color(255, 228, 240, 245), sf::Color(235, 151, 188), 1.5f, 14.f);
            drawText("Vague " + std::to_string(shownWave), { 145.f, 18.f }, 15,
                sf::Color(119, 55, 83));
        }

        {
            const bool broke = economy->unpaidUnits;
            RtsUI::drawRoundedRect(m_window, sf::FloatRect({ 364.f, 12.f }, { 210.f, 32.f }),
                broke ? sf::Color(255, 205, 205, 245) : sf::Color(255, 228, 240, 245),
                broke ? sf::Color(214, 90, 90) : sf::Color(235, 151, 188), 1.5f, 14.f);
            drawText("Salaires  -" + std::to_string(economy->wagePerCycle) + " / "
                + std::to_string(std::max(0, static_cast<int>(std::ceil(economy->wageTimer)))) + " s",
                { 377.f, 18.f }, 15, broke ? sf::Color(150, 40, 40) : sf::Color(119, 55, 83));
        }

        std::string title;
        if (selected.index != UINT32_MAX && registry.hasComponent<ECS::Building>(selected)) {
            const ECS::Building& building = registry.getComponent<ECS::Building>(selected);
            switch (building.type) {
            case ECS::Building::Type::Headquarters: title = "Quartier general"; break;
            case ECS::Building::Type::Farm: title = "Ferme"; break;
            case ECS::Building::Type::Barracks: title = "Caserne"; break;
            case ECS::Building::Type::Tower: title = "Tour de guet"; break;
            }
            if (building.type == ECS::Building::Type::Farm) {
                const sf::FloatRect incomeTag({ static_cast<float>(selectedPixel.x) - 45.f,
                    static_cast<float>(selectedPixel.y) - 58.f }, { 90.f, 25.f });
                RtsUI::drawRoundedRect(m_window, incomeTag, sf::Color(255, 235, 243, 245),
                    sf::Color(232, 157, 190), 1.f, 11.f);
                const float perFarm = economy->farmCount > 0
                    ? economy->incomePerSecond / static_cast<float>(economy->farmCount) : 0.f;
                drawText("+" + rate(building.constructionProgress >= 1.f ? perFarm : 0.f) + " or/s",
                    incomeTag.position + sf::Vector2f(7.f, 4.f), 13, sf::Color(119, 55, 83));
            }
            else if (building.constructionProgress < 1.f) {
                const sf::FloatRect status({ static_cast<float>(selectedPixel.x) - 56.f,
                    static_cast<float>(selectedPixel.y) - 66.f }, { 112.f, 32.f });
                RtsUI::drawRoundedRect(m_window, status, sf::Color(255, 235, 243, 245),
                    sf::Color(232, 157, 190), 1.f, 12.f);
                drawText(std::to_string(building.assignedBuilderCount) + "x  "
                    + std::to_string(static_cast<int>(building.constructionProgress * 100.f)) + "%",
                    status.position + sf::Vector2f(8.f, 3.f), 13, sf::Color(119, 55, 83));
                sf::RectangleShape progressBack({ 96.f, 5.f });
                progressBack.setPosition(status.position + sf::Vector2f(8.f, 22.f));
                progressBack.setFillColor(sf::Color(238, 196, 213));
                m_window.draw(progressBack);
                sf::RectangleShape progressFill({ 96.f * building.constructionProgress, 5.f });
                progressFill.setPosition(progressBack.getPosition());
                progressFill.setFillColor(sf::Color(235, 125, 174));
                m_window.draw(progressFill);
            }
            if (!building.recruitmentQueue.empty()) {
                const ECS::Building::RecruitmentType next = building.recruitmentQueue.front();
                const int duration = next == ECS::Building::RecruitmentType::Archer ? 7 : 5;
                const int remaining = std::max(0, static_cast<int>(std::ceil(
                    duration - building.recruitmentProgress)));
                const sf::FloatRect queueTag({ static_cast<float>(selectedPixel.x) - 72.f,
                    static_cast<float>(selectedPixel.y) - 88.f }, { 144.f, 25.f });
                RtsUI::drawRoundedRect(m_window, queueTag, sf::Color(255, 235, 243, 245),
                    sf::Color(232, 157, 190), 1.f, 11.f);
                drawText("File " + std::to_string(building.recruitmentQueue.size())
                    + " · " + std::to_string(remaining) + " s",
                    queueTag.position + sf::Vector2f(8.f, 4.f), 13, sf::Color(119, 55, 83));
            }
        }
        else if (selected.index != UINT32_MAX && registry.hasComponent<ECS::Unit>(selected)) {
            const ECS::Unit& unit = registry.getComponent<ECS::Unit>(selected);
            if (unit.type == ECS::Unit::Type::Builder) title = "Batisseur";
        }

        if (!hud->buttons.empty()) {
            const sf::FloatRect panel = panelBounds(size, hud->buttons.size());
            RtsUI::drawRoundedRect(m_window, panel, sf::Color(255, 238, 246, 238),
                sf::Color(235, 166, 197), 1.5f, 16.f);
            drawText(title, { panel.position.x + 13.f, panel.position.y + 10.f }, 15,
                sf::Color(119, 55, 83));
        }

        for (const ECS::RtsHudState::Button& button : hud->buttons) {
            RtsUI::drawRoundedRect(m_window, button.bounds, sf::Color(250, 199, 221),
                sf::Color(237, 151, 187), 1.f, 12.f);
            drawText(button.label, button.bounds.position + sf::Vector2f(10.f, 6.f), 13,
                sf::Color(112, 49, 78));
        }

        drawGoldTooltip(*economy);

        m_window.setView(previousView);
    }

    static sf::FloatRect panelBounds(sf::Vector2u windowSize, size_t buttonCount) {
        return sf::FloatRect({ static_cast<float>(windowSize.x) - 214.f, 12.f },
            { 202.f, 42.f + static_cast<float>(buttonCount) * 34.f });
    }

    static void refreshButtons(Registry& registry, ECS::RtsHudState& hud, sf::Vector2u windowSize) {
        hud.buttons.clear();
        const Entity selected = selectedPlayer(registry);
        const float x = static_cast<float>(windowSize.x) - 206.f;
        float y = 57.f;
        auto add = [&](const std::string& label, ECS::RtsHudState::Action action) {
            hud.buttons.push_back({ sf::FloatRect({ x, y }, { 186.f, 28.f }), label, action });
            y += 34.f;
        };
        if (hud.placementMode != ECS::RtsHudState::PlacementMode::None) {
            add("Annuler le placement", ECS::RtsHudState::Action::CancelPlacement);
            return;
        }
        if (selected.index == UINT32_MAX) return;
        if (registry.hasComponent<ECS::Building>(selected)) {
            const ECS::Building& building = registry.getComponent<ECS::Building>(selected);
            if (building.constructionProgress < 1.f) return;
            if (building.type == ECS::Building::Type::Headquarters)
                add("Batisseur - 100 or · 5 s", ECS::RtsHudState::Action::RecruitBuilder);
            else if (building.type == ECS::Building::Type::Barracks) {
                add("Guerrier - 100 or · 5 s", ECS::RtsHudState::Action::RecruitWarrior);
                add("Archer - 150 or · 7 s", ECS::RtsHudState::Action::RecruitArcher);
            }
        }
        else if (registry.hasComponent<ECS::Unit>(selected)
            && registry.getComponent<ECS::Unit>(selected).type == ECS::Unit::Type::Builder) {
            add("Ferme - 150 or", ECS::RtsHudState::Action::BuildFarm);
            add("Caserne - 250 or", ECS::RtsHudState::Action::BuildBarracks);
            add("Tour de guet - 200 or", ECS::RtsHudState::Action::BuildTower);
        }
    }

private:
    sf::RenderWindow& m_window;
    const InputState& m_input;
    sf::Font m_font;
    bool m_hasFont = false;

    static Entity selectedPlayer(Registry& registry) {
        Entity selected = NULL_ENTITY;
        registry.view<ECS::Selectable>([&](Entity entity, ECS::Selectable& selectable) {
            if (selected.index == UINT32_MAX && selectable.playerOwned && selectable.selected)
                selected = entity;
        });
        return selected;
    }

    // un chiffre apres la virgule
    static std::string rate(float value) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.1f", value);
        return buffer;
    }

    // infobulle quand la souris (ou le curseur manette) est sur l'or
    void drawGoldTooltip(const ECS::PlayerEconomy& economy) {
        const sf::FloatRect goldPill({ 12.f, 12.f }, { 112.f, 32.f });
        const sf::Vector2f cursor = m_input.gamepadConnected
            ? m_input.mouseScreenPos
            : sf::Vector2f(sf::Mouse::getPosition(m_window));
        if (!goldPill.contains(cursor)) return;

        const float wagePerSecond = economy.wageInterval > 0.f
            ? static_cast<float>(economy.wagePerCycle) / economy.wageInterval : 0.f;
        const float net = economy.incomePerSecond - wagePerSecond;
        const sf::Color text(119, 55, 83);
        const sf::Color good(52, 135, 86);
        const sf::Color bad(190, 55, 55);

        const sf::FloatRect box({ goldPill.position.x, goldPill.position.y + goldPill.size.y + 6.f },
            { 230.f, 118.f });
        RtsUI::drawRoundedRect(m_window, box, sf::Color(255, 238, 246, 245),
            sf::Color(235, 151, 188), 1.5f, 12.f);

        const float x = box.position.x + 12.f;
        const float xRight = box.position.x + box.size.x - 12.f;
        float y = box.position.y + 8.f;
        auto line = [&](const std::string& label, const std::string& value, sf::Color color) {
            drawText(label, { x, y }, 13, text);
            drawTextRight(value, { xRight, y }, 13, color);
            y += 21.f;
        };
        drawText("Or par seconde", { x, y }, 14, sf::Color(151, 67, 103));
        y += 24.f;
        line("Fermes (" + std::to_string(economy.farmCount) + ")", "+" + rate(economy.incomePerSecond), good);
        line("Salaires", "-" + rate(wagePerSecond), bad);
        line("Net", (net >= 0.f ? "+" : "") + rate(net), net >= 0.f ? good : bad);
        line("Prochaine ferme", "+" + rate(ECS::farmYield(economy.farmCount + 1)), text);
    }

    void drawTextRight(const std::string& value, sf::Vector2f rightEdge, unsigned int size, sf::Color color) {
        if (!m_hasFont || value.empty()) return;
        sf::Text t(m_font, value, size);
        t.setFillColor(color);
        const auto bounds = t.getLocalBounds();
        t.setPosition({ rightEdge.x - bounds.position.x - bounds.size.x, rightEdge.y });
        m_window.draw(t);
    }

    void drawText(const std::string& value, sf::Vector2f position, unsigned int size, sf::Color color) {
        if (!m_hasFont || value.empty()) return;
        sf::Text text(m_font, value, size);
        text.setPosition(position);
        text.setFillColor(color);
        m_window.draw(text);
    }
};
