#include "../classes/ANARSettingsPopup.hpp"
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(ANARPlayLayer, PlayLayer) {
    struct Fields {
        GJGameLevel *m_level = nullptr;
    };

    static void onModify(ModifyBase<ModifyDerive<ANARPlayLayer, PlayLayer>>& self) {
        (void)self.getHook("PlayLayer::destroyPlayer").map([](Hook* hook) {
            auto mod = Mod::get();
            hook->setAutoEnable(mod->getSettingValue<bool>("enable"));

            listenForSettingChangesV3<bool>("enable", [hook](bool value) {
                (void)(value ? hook->enable().mapErr([](const std::string& err) {
                    return log::error("Failed to enable PlayLayer::destroyPlayer hook: {}", err), err;
                }) : hook->disable()).mapErr([](const std::string& err) {
                    return log::error("Failed to disable PlayLayer::destroyPlayer hook: {}", err), err;
                });
            }, mod);

            return hook;
        }).mapErr([](const std::string& err) {
            return log::error("Failed to get PlayLayer::destroyPlayer hook: {}", err), err;
        });
    }

    void resetLevel() {
        PlayLayer::resetLevel();

        if (auto popup = CCScene::get()->getChildByType<ANARSettingsPopup>(0)) popup->onClose(nullptr);
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        auto mod = Mod::get();
       
        auto GM = GameManager::get();
        auto autoRetryEnabled = GM->getGameVariable("0026");

        if (!autoRetryEnabled) {
            PlayLayer::destroyPlayer(player, object);
            return;
        }

        // checks if new best option is enabled - if so, sets the m_level field to the current level
        if (mod->getSettingValue<bool>("new-best")) {
            log::info("Playing level with new best setting enabled");
            m_fields->m_level = this->m_level;
        } else {
            log::warn("Playing level with new best setting disabled");
            m_fields->m_level = nullptr;
        }

        // checks if m_level field exists - if not, uses the percent setting
        auto setPercentage = m_fields->m_level
                                 ? m_fields->m_level->m_normalPercent.value()
                                 : as<int>(mod->getSettingValue<int64_t>("percentage"));
        auto startPosIsOk = player->m_isStartPos ? player->m_isStartPos && mod->getSettingValue<bool>("start-pos") : true; // if the player wants no auto retry on startpos

        if ((!m_isPracticeMode && !player->m_isPlatformer && startPosIsOk) && (this->getCurrentPercentInt() >= setPercentage))
            GM->setGameVariable("0026", false);

        PlayLayer::destroyPlayer(player, object);

        GM->setGameVariable("0026", autoRetryEnabled);
    }
};