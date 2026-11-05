#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <vector>

using namespace geode::prelude;

// Global Cheat & Configuration States
bool g_noclip = true;
bool g_inf_jump = false;
float g_speedhack = 1.0f;

// Struct to track player actions inside the macro bot engine
struct MacroAction {
    float frame;
    int playerButton; 
    bool isPress;
    bool isPlayer2;
};

// Global container for the currently loaded macro data
std::vector<MacroAction> g_savedMacroData;

// Custom Confirmation UI Delegate for the structural safe macro deleting wrapper
class MacroDeleteConfirmDelegate : public FLAlertLayerProtocol {
public:
    void FLAlert_Clicked(FLAlertLayer* layer, int btnIndex) override {
        if (btnIndex == 1) { // User clicked 'Yes'
            g_savedMacroData.clear();
            FLAlertLayer::create("Macro Engine", "Macro <cr>deleted</c> successfully.", "OK")->show();
        }
    }
};
static MacroDeleteConfirmDelegate* g_deleteDelegate = new MacroDeleteConfirmDelegate();

// Speedhack Implementation: Intercepting the delta time matrix loop
class $modify(MySpeedhackScheduler, CCScheduler) {
    void update(float dt) {
        // Enforce boundary protections globally
        float targetSpeed = g_speedhack;
        if (targetSpeed < 0.00007f) targetSpeed = 0.00007f;
        if (targetSpeed > 148.4f) targetSpeed = 148.4f;

        // Multiply the frame timing step by the safe speed hack limits
        CCScheduler::update(dt * targetSpeed);
    }
};

class $modify(MySafeCheatLayer, PlayLayer) {
    // Dynamic macro state trackers
    bool m_isRecording = false;
    bool m_isPlaying = false;
    float m_currentFrame = 0.0f;
    size_t m_playbackIndex = 0;
    std::vector<MacroAction> m_recordedActions;
    CCLabelBMFont* m_safeModeLabel = nullptr;

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        // Force Enforced Warning Visible
        m_fields->m_safeModeLabel = CCLabelBMFont::create("SAFE MODE ENFORCED", "goldFont.fnt");
        if (m_fields->m_safeModeLabel) {
            auto winSize = CCDirector::sharedDirector()->getWinSize();
            m_fields->m_safeModeLabel->setPosition({ winSize.width / 2, winSize.height - 20 });
            m_fields->m_safeModeLabel->setScale(0.6f);
            m_fields->m_safeModeLabel->setColor({ 255, 100, 100 });
            this->addChild(m_fields->m_safeModeLabel, 100);
        }

        // Generate Bot Control Interface Overlay
        this->setupMacroUI();

        return true;
    }

    void setupMacroUI() {
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto menu = CCMenu::create();
        menu->setPosition({40, winSize.height / 2});
        
        // Define Button Factory Lambda
        auto createBtn = [&](const char* labelStr, SEL_MenuHandler selector) {
            auto label = CCLabelBMFont::create(labelStr, "bigFont.fnt");
            label->setScale(0.35f);
            auto btn = CCMenuItemSpriteExtra::create(label, this, selector);
            return btn;
        };

        menu->addChild(createBtn("NEW REC", menu_selector(MySafeCheatLayer::onNewMacro)));
        menu->addChild(createBtn("SAVE", menu_selector(MySafeCheatLayer::onSaveMacro)));
        menu->addChild(createBtn("LOAD", menu_selector(MySafeCheatLayer::onLoadMacro)));
        menu->addChild(createBtn("DELETE", menu_selector(MySafeCheatLayer::onDeleteMacro)));

        menu->alignItemsVerticallyWithPadding(8.f);
        this->addChild(menu, 101);
    }

    // --- Macro UI Callback Directives ---
    void onNewMacro(CCObject*) {
        m_fields->m_recordedActions.clear();
        m_fields->m_isRecording = true;
        m_fields->m_isPlaying = false;
        m_fields->m_currentFrame = 0.0f;
        FLAlertLayer::create("Macro Engine", "Recording <cg>started</c>!", "OK")->show();
    }

    void onSaveMacro(CCObject*) {
        m_fields->m_isRecording = false;
        g_savedMacroData = m_fields->m_recordedActions;
        FLAlertLayer::create("Macro Engine", "Current recording <cl>saved to memory</c>.", "OK")->show();
    }

    void onLoadMacro(CCObject*) {
        if (g_savedMacroData.empty()) {
            FLAlertLayer::create("Macro Engine", "No saved macro found in database.", "OK")->show();
            return;
        }
        m_fields->m_recordedActions = g_savedMacroData;
        m_fields->m_isPlaying = true;
        m_fields->m_isRecording = false;
        m_fields->m_currentFrame = 0.0f;
        m_fields->m_playbackIndex = 0;
        FLAlertLayer::create("Macro Engine", "Macro data <cg>loaded</c>. Active playback prepared.", "OK")->show();
    }

    void onDeleteMacro(CCObject*) {
        // Enforcing safety check workflow: prompt validation layout instead of quick clearance
        auto alert = FLAlertLayer::create(
            g_deleteDelegate,
            "Are you sure?",
            "This will completely erase the macro trace data. Proceed?",
            "Cancel",
            "Yes"
        );
        alert->show();
    }

    // Hook action buttons to log actions when recording macros
    void pushButton(PlayerButton btn, bool isPlayer2) {
        PlayLayer::pushButton(btn, isPlayer2);
        if (m_fields->m_isRecording) {
            m_fields->m_recordedActions.push_back({m_fields->m_currentFrame, static_cast<int>(btn), true, isPlayer2});
        }
    }

    void releaseButton(PlayerButton btn, bool isPlayer2) {
        PlayLayer::releaseButton(btn, isPlayer2);
        if (m_fields->m_isRecording) {
            m_fields->m_recordedActions.push_back({m_fields->m_currentFrame, static_cast<int>(btn), false, isPlayer2});
        }
    }

    // Main Gameplay Logic Frame Updates
    void update(float dt) {
        PlayLayer::update(dt);
        
        m_fields->m_currentFrame += 1.0f;

        // Auto Safe Mode validation triggers
        bool anyCheatActive = g_noclip || g_inf_jump || (g_speedhack != 1.0f) || m_fields->m_isPlaying || m_fields->m_isRecording;
        if (m_fields->m_safeModeLabel) {
            m_fields->m_safeModeLabel->setVisible(anyCheatActive);
        }

        // Macro Playback Processing Engine Loop
        if (m_fields->m_isPlaying && m_fields->m_playbackIndex < m_fields->m_recordedActions.size()) {
            auto& action = m_fields->m_recordedActions[m_fields->m_playbackIndex];
            if (m_fields->m_currentFrame >= action.frame) {
                if (action.isPress) {
                    PlayLayer::pushButton(static_cast<PlayerButton>(action.playerButton), action.isPlayer2);
                } else {
                    PlayLayer::releaseButton(static_cast<PlayerButton>(action.playerButton), action.isPlayer2);
                }
                m_fields->m_playbackIndex++;
            }
        }

        // Infinite Jump Logic
        if (g_inf_jump) {
            if (this->m_player1) this->m_player1->m_isGrounded = true;
            if (this->m_player2) this->m_player2->m_isGrounded = true;
        }
    }

    // Hard-Forced Level Progression Guard Rail Interception
    void levelComplete() {
        bool anyCheatActive = g_noclip || g_inf_jump || (g_speedhack != 1.0f) || m_fields->m_isPlaying || m_fields->m_isRecording;
        if (anyCheatActive) {
            FLAlertLayer::create(
                "Safe Mode Active",
                "Your progress was <cr>fully protected</c>.\nCheats or Macro modifications were active.",
                "OK"
            )->show();
            this->onQuit(); 
            return;
        }
        PlayLayer::levelComplete();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (g_noclip) return; 
        PlayLayer::destroyPlayer(player, object);
    }
};

// Hook runtime options settings parameters dynamically on execution startup
$execute {
    geode::Mod::get()->listenSettingChanged("toggle-noclip", [](bool value) {
        g_noclip = value;
    });
    geode::Mod::get()->listenSettingChanged("toggle-inf-jump", [](bool value) {
        g_inf_jump = value;
    });
    geode::Mod::get()->listenSettingChanged("speed-multiplier", [](double value) {
        g_speedhack = static_cast<float>(value);
    });
}
