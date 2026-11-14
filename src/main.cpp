#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/ui/Notification.hpp>
#include <fstream>
#include <filesystem>

using namespace geode::prelude;

struct Input { int frame; bool down; bool p1; };

static std::vector<Input> g_macro;
static size_t g_idx = 0;
static bool g_touchHeld = false;
static bool g_gateWasOpen = false;

static std::string mode() { return Mod::get()->getSettingValue<std::string>("mode"); }
static bool recording() { return mode() == "Record"; }
static bool playing() { return mode() == "Hold Play"; }

static std::filesystem::path macroPath(GJGameLevel* lvl) {
    auto dir = Mod::get()->getSaveDir() / "macros";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir / (std::to_string(lvl->m_levelID.value()) + ".txt");
}

static void loadMacro(GJGameLevel* lvl) {
    g_macro.clear();
    std::ifstream f(macroPath(lvl));
    int fr, d, p;
    while (f >> fr >> d >> p) g_macro.push_back({fr, d != 0, p != 0});
}

static void saveMacro(GJGameLevel* lvl) {
    std::ofstream f(macroPath(lvl), std::ios::trunc);
    for (auto& i : g_macro) f << i.frame << ' ' << (i.down ? 1 : 0) << ' ' << (i.p1 ? 1 : 0) << '\n';
}

class $modify(HBPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        g_idx = 0; g_touchHeld = false; g_gateWasOpen = false;
        if (playing()) loadMacro(m_level); else g_macro.clear();
    }
    void resetLevel() {
        PlayLayer::resetLevel();
        g_idx = 0; g_touchHeld = false; g_gateWasOpen = false;
        if (recording()) g_macro.clear();
    }
    void levelComplete() {
        if (recording() && !g_macro.empty() && !m_isTestMode) {
            saveMacro(m_level);
            Notification::create("Hold Bot: macro saved", NotificationIcon::Success)->show();
        }
        PlayLayer::levelComplete();
    }
};

class $modify(HBBaseLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (!PlayLayer::get()) return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        if (recording() && button == 1) {
            g_macro.push_back({m_gameState.m_currentProgress, down, isPlayer1});
        }
        if (playing()) {
            // real touch is only the "enable" switch, the bot does the real inputs
            g_touchHeld = down;
            return; // user's taps never reach the player
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        if (PlayLayer::get() && playing() && !m_player1->m_isDead) {
            bool need = Mod::get()->getSettingValue<bool>("need-hold");
            bool open = !need || g_touchHeld;
            int step = m_gameState.m_currentProgress;
            if (open) {
                // apply everything due (including inputs skipped while not holding), last state wins
                int last1 = -1, last2 = -1;
                while (g_idx < g_macro.size() && g_macro[g_idx].frame <= step) {
                    auto& in = g_macro[g_idx++];
                    (in.p1 ? last1 : last2) = in.down ? 1 : 0;
                }
                if (last1 != -1) GJBaseGameLayer::handleButton(last1 == 1, 1, true);
                if (last2 != -1) GJBaseGameLayer::handleButton(last2 == 1, 1, false);
            } else if (g_gateWasOpen) {
                GJBaseGameLayer::handleButton(false, 1, true);
                GJBaseGameLayer::handleButton(false, 1, false);
            }
            g_gateWasOpen = open;
        }
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};
