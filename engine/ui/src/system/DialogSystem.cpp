#include <fstream>

#include "system/Dialog_system.h"
#include "../../Include/components/Dialog.h"
#include "utils/camera_utils.h"
#include "scene/component_serializer.h"
#include "ecs/registry.h"
#include  <iostream>
#include <numbers>

#include "components/UIButton.h"
#include "components/UIText.h"
#include "components/collider2d.h"
#include "components/dont_destroy.h"
#include "components/geometry.h"
#include "components/interaction.h"
#include "components/map_transition.h"
#include "systems/camera_system.h"

DialogSystem::DialogSystem(Game& game, InteractionSystem& interaction) : m_game(game) {
    priority = 14;
    interaction.addListener([this](Registry& registry, Entity who, Entity target) { use(registry, who, target);});
}

void DialogSystem::use(Registry &registry, Entity entityA, Entity entityB) {
    if (!registry.hasComponent<Interactable>(entityA) && !registry.hasComponent<Interactable>(entityB))return;
    if (!registry.hasComponent<DontDestroy>(entityA) && !registry.hasComponent<DontDestroy>(entityB))return;
    if (registry.hasComponent<Dialog>(entityA)){registry.getComponent<Dialog>(entityA).currentLine++; return;}
    if (registry.hasComponent<Dialog>(entityB))registry.getComponent<Dialog>(entityB).currentLine++;
}

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
        if (dialog.currentLine < 0){ui.text = ui.defaultText;return;}
        if (dialog.currentLine >= dialog.dialogs.size()){dialog.currentLine = -1;return;}
        ui.text = dialog.dialogs[dialog.currentLine];
        });
    }
