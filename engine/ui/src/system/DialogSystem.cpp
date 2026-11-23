#include <fstream>

#include "system/Dialog_system.h"
#include "../../Include/components/Dialog.h"
#include "utils/camera_utils.h"
#include "scene/component_serializer.h"
#include "ecs/registry.h"
#include  <iostream>

#include "components/UIButton.h"
#include "components/UIText.h"
#include "components/collider2d.h"
#include "components/dont_destroy.h"
#include "components/geometry.h"
#include "components/map_transition.h"
#include "systems/camera_system.h"

DialogSystem::DialogSystem(sf::RenderTarget &renderTarget, Game &game) : m_game(game), m_rectShape(new sf::RectangleShape({100,50})), m_renderTarget(renderTarget), m_font(new sf::Font()), m_text(new sf::Text(*m_font))
{
    m_font->openFromFile("C:\\Windows\\Fonts\\arial.ttf");
    m_text->setFont(*m_font);
    m_text->setCharacterSize(50);
    priority = 14;
};

void DialogSystem::onUpdate(Registry &registry, float dt) {
    registry.view<Dialog, UIText, Transform2D>([&](Entity e, Dialog& dialog, UIText& ui, Transform2D& transform) {
        if (dialog.dialogs.empty())
        {
            std::ifstream in(m_game.assetDir()+"/dialogs/"+dialog.filePath);
            if (!in) return;
            std::ostringstream ss;
            ss << in.rdbuf();
            const Json j = Json::parse( ss.str(), nullptr, false);
            if (j.is_discarded() || !j.is_object()) return;
            if (j.value("format", std::string()) != "angeVault.dialog")return;
             // = Json::array();
            if (j.contains("dialog")) {
                Json J = j["dialog"];
                for (int i = 0; i < J.size(); ++i)
                {
                        dialog.dialogs.emplace_back(J[i].get<std::string>());
                }
            }
        }

        if (dialog.currentLine >= dialog.dialogs.size())dialog.currentLine = 0;
        ui.text = dialog.dialogs[dialog.currentLine++];
        m_text->setString(ui.text);
        m_renderTarget.draw(*m_text, transform.getLocalMatrix());
        });
    }