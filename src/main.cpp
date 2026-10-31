#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <random>
#include <string>
#include <vector>

using namespace geode::prelude;

static const char* const kFrames[6] = {
    "diffIcon_06_btn_001.png", "diffIcon_07_btn_001.png",
    "diffIcon_08_btn_001.png", "diffIcon_06_btn_001.png",
    "diffIcon_09_btn_001.png", "diffIcon_10_btn_001.png"
};
static const char* const kNames[6] = {
    "Demon", "Easy", "Medium", "Hard", "Insane", "Extreme"
};
static const char* const kLengths[5] = {
    "Short", "Medium", "Long", "XL", "Plat."
};
static const char* const kFilterKeys[3] = {
    "filter-rated", "filter-featured", "filter-epic"
};
static const char* const kFilterNames[3] = {
    "Rated only", "Featured only", "Epic only"
};
static const ccColor4F kWedge[6] = {
    { 0.75f, 0.15f, 0.15f, 1.f },
    { 0.35f, 0.25f, 0.75f, 1.f },
    { 0.65f, 0.20f, 0.65f, 1.f },
    { 0.85f, 0.30f, 0.20f, 1.f },
    { 0.90f, 0.55f, 0.15f, 1.f },
    { 0.55f, 0.05f, 0.10f, 1.f }
};
static constexpr float kPi = 3.14159265f;
static constexpr float kWheelY = 112.f;
static constexpr float kWheelR = 40.f;

// Plays one of Geometry Dash's built-in sound effects (if sounds are enabled)
static void playSfx(const char* name, float volume = 1.f, float speed = 1.f) {
    if (!Mod::get()->getSettingValue<bool>("sounds")) return;
    FMODAudioEngine::sharedEngine()->playEffect(name, speed, 0.f, volume);
}

// Draws a round circle (the built-in drawDot looks square)
static void drawCircle(CCDrawNode* node, CCPoint c, float r, ccColor4F fill) {
    std::vector<CCPoint> pts;
    for (int i = 0; i < 40; i++) {
        float a = i * (kPi * 2.f / 40.f);
        pts.push_back({ c.x + std::cos(a) * r, c.y + std::sin(a) * r });
    }
    node->drawPolygon(pts.data(), static_cast<unsigned int>(pts.size()), fill, 0.f, fill);
}

// ======================= Stats and streak =======================
// A number that goes up by 1 every day (uses your local date)
static int todayNumber() {
    std::time_t now = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    int y = tmv.tm_year + 1900;
    int m = tmv.tm_mon + 1;
    int d = tmv.tm_mday;
    if (m <= 2) y -= 1;
    int era = (y >= 0 ? y : y - 399) / 400;
    int yoe = y - era * 400;
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

// The streak you currently have (0 if you missed a day)
static int currentStreak() {
    auto mod = Mod::get();
    int last = mod->getSavedValue<int>("streak-last-day", -1);
    int today = todayNumber();
    if (last == today || last == today - 1) {
        return mod->getSavedValue<int>("streak-current");
    }
    return 0;
}

static void recordSpin() {
    auto mod = Mod::get();
    mod->setSavedValue<int>("stat-spins", mod->getSavedValue<int>("stat-spins") + 1);
}

// Called when you open a demon from the roulette
static void recordOpen() {
    auto mod = Mod::get();
    mod->setSavedValue<int>("stat-opened", mod->getSavedValue<int>("stat-opened") + 1);

    int today = todayNumber();
    int last = mod->getSavedValue<int>("streak-last-day", -1);
    int cur = mod->getSavedValue<int>("streak-current");

    if (last == today) {
        if (cur < 1) cur = 1;
    } else if (last == today - 1) {
        cur += 1;
    } else {
        cur = 1;
    }

    int best = mod->getSavedValue<int>("streak-best");
    if (cur > best) best = cur;

    mod->setSavedValue<int>("streak-current", cur);
    mod->setSavedValue<int>("streak-best", best);
    mod->setSavedValue<int>("streak-last-day", today);
}

// ======================= Stats popup =======================
class StatsPopup : public Popup {
protected:
    bool init() {
        if (!Popup::init(260.f, 190.f)) return false;

        this->setTitle("Stats");

        auto mod = Mod::get();
        std::string lines[4] = {
            fmt::format("Spins: {}", mod->getSavedValue<int>("stat-spins")),
            fmt::format("Demons opened: {}", mod->getSavedValue<int>("stat-opened")),
            fmt::format("Current streak: {}", currentStreak()),
            fmt::format("Best streak: {}", mod->getSavedValue<int>("streak-best"))
        };

        for (int i = 0; i < 4; i++) {
            auto lbl = CCLabelBMFont::create(lines[i].c_str(), "bigFont.fnt");
            lbl->setScale(0.5f);
            lbl->limitLabelWidth(220.f, 0.5f, 0.2f);
            lbl->setPosition({ m_size.width / 2, 130.f - i * 30.f });
            m_mainLayer->addChild(lbl);
        }
        return true;
    }

public:
    static StatsPopup* create() {
        auto ret = new StatsPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ======================= Filters popup =======================
class FiltersPopup : public Popup {
protected:
    bool init() {
        if (!Popup::init(220.f, 170.f)) return false;

        this->setTitle("Filters");

        auto menu = CCMenu::create();
        menu->setPosition({ 0.f, 0.f });
        m_mainLayer->addChild(menu);

        for (int i = 0; i < 3; i++) {
            float y = 110.f - i * 36.f;

            auto toggler = CCMenuItemToggler::createWithStandardSprites(
                this, menu_selector(FiltersPopup::onToggle), 0.8f
            );
            toggler->setTag(i);
            toggler->toggle(Mod::get()->getSavedValue<bool>(kFilterKeys[i]));
            toggler->setPosition({ 40.f, y });
            menu->addChild(toggler);

            auto lbl = CCLabelBMFont::create(kFilterNames[i], "bigFont.fnt");
            lbl->setScale(0.5f);
            lbl->setAnchorPoint({ 0.f, 0.5f });
            lbl->setPosition({ 65.f, y });
            m_mainLayer->addChild(lbl);
        }
        return true;
    }

    void onToggle(CCObject* sender) {
        auto toggler = static_cast<CCMenuItemToggler*>(sender);
        // The toggler flips after this runs, so the new value is the opposite
        bool now = !toggler->isToggled();
        Mod::get()->setSavedValue<bool>(kFilterKeys[toggler->getTag()], now);
    }

public:
    static FiltersPopup* create() {
        auto ret = new FiltersPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ======================= History popup =======================
class HistoryPopup : public Popup {
protected:
    bool init() {
        if (!Popup::init(300.f, 250.f)) return false;

        this->setTitle("History");

        auto menu = CCMenu::create();
        menu->setPosition({ 0.f, 0.f });
        m_mainLayer->addChild(menu);

        auto ids = Mod::get()->getSavedValue<std::vector<int>>("history-ids");
        auto names = Mod::get()->getSavedValue<std::vector<std::string>>("history-names");

        if (ids.empty() || ids.size() != names.size()) {
            auto lbl = CCLabelBMFont::create("No spins yet!", "bigFont.fnt");
            lbl->setScale(0.6f);
            lbl->setPosition({ m_size.width / 2, m_size.height / 2 + 10.f });
            m_mainLayer->addChild(lbl);
        } else {
            int shown = std::min<int>(7, static_cast<int>(ids.size()));
            for (int k = 0; k < shown; k++) {
                int idx = static_cast<int>(ids.size()) - 1 - k;
                auto lbl = CCLabelBMFont::create(names[idx].c_str(), "bigFont.fnt");
                lbl->setScale(0.5f);
                lbl->limitLabelWidth(250.f, 0.5f, 0.2f);
                auto btn = CCMenuItemSpriteExtra::create(
                    lbl, this, menu_selector(HistoryPopup::onEntry)
                );
                btn->setTag(ids[idx]);
                btn->setPosition({ m_size.width / 2, 200.f - k * 24.f });
                menu->addChild(btn);
            }
        }

        auto clearSpr = ButtonSprite::create("Clear");
        clearSpr->setScale(0.8f);
        auto clearBtn = CCMenuItemSpriteExtra::create(
            clearSpr, this, menu_selector(HistoryPopup::onClear)
        );
        clearBtn->setPosition({ 80.f, 25.f });
        menu->addChild(clearBtn);

        auto resetSpr = ButtonSprite::create("Reset Seen");
        resetSpr->setScale(0.8f);
        auto resetBtn = CCMenuItemSpriteExtra::create(
            resetSpr, this, menu_selector(HistoryPopup::onResetSeen)
        );
        resetBtn->setPosition({ 205.f, 25.f });
        menu->addChild(resetBtn);

        return true;
    }

    void onEntry(CCObject* sender) {
        int id = static_cast<CCNode*>(sender)->getTag();
        auto search = GJSearchObject::create(SearchType::Search, std::to_string(id));
        CCDirector::get()->pushScene(
            CCTransitionFade::create(0.5f, LevelBrowserLayer::scene(search))
        );
    }

    void onClear(CCObject*) {
        Mod::get()->setSavedValue<std::vector<int>>("history-ids", std::vector<int>{});
        Mod::get()->setSavedValue<std::vector<std::string>>("history-names", std::vector<std::string>{});
        this->onClose(nullptr);
    }

    // Lets the roulette give you levels it already gave you before
    void onResetSeen(CCObject*) {
        Mod::get()->setSavedValue<std::vector<int>>("seen-levels", std::vector<int>{});
        this->onClose(nullptr);
    }

public:
    static HistoryPopup* create() {
        auto ret = new HistoryPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ======================= Main roulette popup =======================
struct HoverItem {
    CCMenuItemSpriteExtra* btn;
    CCNode* visual;
    float base;
    bool hovered;
};

class RoulettePopup : public Popup, public LevelManagerDelegate {
protected:
    CCLabelBMFont* m_resultLabel = nullptr;
    CCLabelBMFont* m_streakLabel = nullptr;
    CCNode* m_wheel = nullptr;
    CCNode* m_rim = nullptr;
    CCDrawNode* m_pointer = nullptr;
    ButtonSprite* m_spinSpr = nullptr;
    CCMenuItemSpriteExtra* m_spinBtn = nullptr;
    CCMenuItemSpriteExtra* m_openBtn = nullptr;
    CCMenuItemSpriteExtra* m_rerollBtn = nullptr;
    CCMenu* m_choiceMenu = nullptr;
    CCLayerColor* m_flash = nullptr;
    std::vector<CCSprite*> m_wheelFaces;
    std::vector<CCSprite*> m_burst;
    std::vector<CCSprite*> m_diffSprites;
    std::vector<CCLabelBMFont*> m_lenLabels;
    std::vector<HoverItem> m_items;
    Ref<GJGameLevel> m_pendingLevel;
    bool m_loading = false;
    int m_selected = 0;
    int m_selectedLen = -1;
    int m_maxPage = 19;
    int m_lastSeg = 0;
    std::mt19937 m_rng{ std::random_device{}() };

    bool init() {
        if (!Popup::init(340.f, 270.f)) return false;

        this->setTitle("Demon Roulette");

        auto menu = CCMenu::create();
        menu->setPosition({ 0.f, 0.f });
        m_mainLayer->addChild(menu, 6);

        // Difficulty row
        for (int i = 0; i < 6; i++) {
            float x = m_size.width / 2 + (i - 2.5f) * 48.f;

            auto spr = CCSprite::createWithSpriteFrameName(kFrames[i]);
            spr->setScale(0.65f);
            auto btn = CCMenuItemSpriteExtra::create(
                spr, this, menu_selector(RoulettePopup::onPickDiff)
            );
            btn->setTag(i);
            btn->setPosition({ x, 232.f });
            menu->addChild(btn);
            m_diffSprites.push_back(spr);
            m_items.push_back({ btn, spr, 0.65f, false });

            auto lbl = CCLabelBMFont::create(kNames[i], "bigFont.fnt");
            lbl->setScale(0.25f);
            lbl->setPosition({ x, 211.f });
            m_mainLayer->addChild(lbl);
        }

        // Length row
        for (int i = 0; i < 5; i++) {
            float x = m_size.width / 2 + (i - 2.f) * 62.f;

            auto lbl = CCLabelBMFont::create(kLengths[i], "bigFont.fnt");
            lbl->setScale(0.36f);
            auto btn = CCMenuItemSpriteExtra::create(
                lbl, this, menu_selector(RoulettePopup::onPickLen)
            );
            btn->setTag(i);
            btn->setPosition({ x, 191.f });
            menu->addChild(btn);
            m_lenLabels.push_back(lbl);
            m_items.push_back({ btn, lbl, 0.36f, false });
        }
        this->updateSelection();

        // Side buttons: Filters, History (left) and Stats, Settings (right)
        auto addSide = [&](const char* text, float x, float y, SEL_MenuHandler cb) {
            auto spr = ButtonSprite::create(text);
            spr->setScale(0.55f);
            auto b = CCMenuItemSpriteExtra::create(spr, this, cb);
            b->setPosition({ x, y });
            menu->addChild(b);
        };
        addSide("Filters", 42.f, 122.f, menu_selector(RoulettePopup::onFilters));
        addSide("History", 42.f, 92.f, menu_selector(RoulettePopup::onHistory));
        addSide("Stats", 298.f, 137.f, menu_selector(RoulettePopup::onStats));
        addSide("Settings", 298.f, 107.f, menu_selector(RoulettePopup::onSettings));

        // Streak counter (right side)
        m_streakLabel = CCLabelBMFont::create("", "goldFont.fnt");
        m_streakLabel->setScale(0.45f);
        m_streakLabel->setPosition({ 298.f, 80.f });
        m_mainLayer->addChild(m_streakLabel, 3);
        this->updateStreakLabel();

        CCPoint center = { m_size.width / 2, kWheelY };

        // ----- Rim with chasing lights (does not rotate) -----
        m_rim = CCNode::create();
        m_rim->setPosition(center);
        m_rim->setScale(0.f);
        m_mainLayer->addChild(m_rim, 0);

        auto rimDraw = CCDrawNode::create();
        drawCircle(rimDraw, { 0.f, 0.f }, 48.f, ccc4f(1.f, 0.8f, 0.2f, 1.f));
        drawCircle(rimDraw, { 0.f, 0.f }, 44.5f, ccc4f(0.25f, 0.12f, 0.05f, 1.f));
        m_rim->addChild(rimDraw);

        const int lightCount = 14;
        for (int i = 0; i < lightCount; i++) {
            float a = i * (kPi * 2.f / lightCount);
            auto light = CCDrawNode::create();
            drawCircle(light, { 0.f, 0.f }, 2.2f, ccc4f(1.f, 0.95f, 0.6f, 1.f));
            light->setPosition({ std::cos(a) * 46.2f, std::sin(a) * 46.2f });
            m_rim->addChild(light);
            light->runAction(CCSequence::create(
                CCDelayTime::create(i * 0.07f),
                CCRepeatForever::create(CCSequence::create(
                    CCScaleTo::create(0.2f, 1.5f),
                    CCScaleTo::create(0.2f, 0.6f),
                    CCDelayTime::create(0.6f),
                    nullptr
                )),
                nullptr
            ));
        }

        // ----- The wheel -----
        m_wheel = CCNode::create();
        m_wheel->setPosition(center);
        m_wheel->setScale(0.f);
        m_mainLayer->addChild(m_wheel, 1);

        auto disc = CCDrawNode::create();
        ccColor4F gold = ccc4f(1.f, 0.8f, 0.2f, 1.f);
        for (int i = 0; i < 6; i++) {
            std::vector<CCPoint> pts;
            pts.push_back({ 0.f, 0.f });
            for (int d = -30; d <= 30; d += 5) {
                float a = (90.f - 60.f * i + d) * kPi / 180.f;
                pts.push_back({ std::cos(a) * kWheelR, std::sin(a) * kWheelR });
            }
            disc->drawPolygon(pts.data(), static_cast<unsigned int>(pts.size()), kWedge[i], 1.f, gold);
        }
        drawCircle(disc, { 0.f, 0.f }, 9.f, gold);
        drawCircle(disc, { 0.f, 0.f }, 5.f, ccc4f(0.8f, 0.1f, 0.1f, 1.f));
        m_wheel->addChild(disc);

        for (int i = 0; i < 6; i++) {
            float a = (90.f - 60.f * i) * kPi / 180.f;
            auto s = CCSprite::createWithSpriteFrameName(kFrames[i]);
            s->setScale(0.5f);
            s->setPosition({ std::cos(a) * 24.f, std::sin(a) * 24.f });
            m_wheel->addChild(s);
            m_wheelFaces.push_back(s);
        }

        // Pointer at the top of the wheel
        m_pointer = CCDrawNode::create();
        CCPoint tri[3] = { { -7.f, 13.f }, { 7.f, 13.f }, { 0.f, -3.f } };
        m_pointer->drawPolygon(tri, 3, ccc4f(1.f, 0.25f, 0.25f, 1.f), 1.f, ccc4f(1.f, 1.f, 1.f, 1.f));
        m_pointer->setPosition({ center.x, center.y + 50.f });
        m_mainLayer->addChild(m_pointer, 4);

        // Burst faces (hidden until a result lands)
        for (int i = 0; i < 10; i++) {
            auto s = CCSprite::createWithSpriteFrameName(kFrames[i % 6]);
            s->setScale(0.4f);
            s->setPosition(center);
            s->setOpacity(0);
            m_mainLayer->addChild(s, 5);
            m_burst.push_back(s);
        }

        // Wheel and rim pop in
        m_rim->runAction(CCSequence::create(
            CCDelayTime::create(0.2f),
            CCEaseBackOut::create(CCScaleTo::create(0.45f, 1.f)),
            nullptr
        ));
        m_wheel->runAction(CCSequence::create(
            CCDelayTime::create(0.2f),
            CCEaseBackOut::create(CCScaleTo::create(0.45f, 1.f)),
            nullptr
        ));
        this->startIdleAnimation();

        // Result text
        m_resultLabel = CCLabelBMFont::create("Press Spin!", "bigFont.fnt");
        m_resultLabel->setScale(0.5f);
        m_resultLabel->setPosition({ m_size.width / 2, 54.f });
        m_mainLayer->addChild(m_resultLabel, 3);

        // Spin button
        m_spinSpr = ButtonSprite::create("Spin");
        m_spinBtn = CCMenuItemSpriteExtra::create(
            m_spinSpr,
            this,
            menu_selector(RoulettePopup::onSpin)
        );
        m_spinBtn->setPosition({ m_size.width / 2, 25.f });
        menu->addChild(m_spinBtn);
        this->pulseSpinButton();

        // Open / Reroll buttons (hidden until a result lands)
        m_choiceMenu = CCMenu::create();
        m_choiceMenu->setPosition({ 0.f, 0.f });
        m_choiceMenu->setVisible(false);
        m_mainLayer->addChild(m_choiceMenu, 7);

        m_openBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Open"),
            this,
            menu_selector(RoulettePopup::onOpen)
        );
        m_openBtn->setPosition({ m_size.width / 2 - 60.f, 25.f });
        m_choiceMenu->addChild(m_openBtn);

        m_rerollBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Reroll"),
            this,
            menu_selector(RoulettePopup::onReroll)
        );
        m_rerollBtn->setPosition({ m_size.width / 2 + 60.f, 25.f });
        m_choiceMenu->addChild(m_rerollBtn);

        // White flash layer (invisible until a result lands)
        m_flash = CCLayerColor::create({ 255, 255, 255, 0 }, m_size.width - 12.f, m_size.height - 12.f);
        m_flash->setPosition({ 6.f, 6.f });
        m_mainLayer->addChild(m_flash, 10);

        this->schedule(schedule_selector(RoulettePopup::hoverTick));
        this->schedule(schedule_selector(RoulettePopup::wheelTick));

        return true;
    }

    void updateStreakLabel() {
        m_streakLabel->setString(fmt::format("Streak: {}", currentStreak()).c_str());
    }

    // ----- side buttons -----
    void onFilters(CCObject*) {
        if (m_loading) return;
        FiltersPopup::create()->show();
    }

    void onHistory(CCObject*) {
        if (m_loading) return;
        HistoryPopup::create()->show();
    }

    void onStats(CCObject*) {
        if (m_loading) return;
        StatsPopup::create()->show();
    }

    void onSettings(CCObject*) {
        openSettingsPopup(Mod::get(), false);
    }

    void updateSelection() {
        for (int i = 0; i < 6; i++) {
            m_diffSprites[i]->setOpacity(i == m_selected ? 255 : 90);
        }
        for (int i = 0; i < 5; i++) {
            m_lenLabels[i]->setOpacity(
                (m_selectedLen == -1 || i == m_selectedLen) ? 255 : 90
            );
        }
    }

    void onPickDiff(CCObject* sender) {
        if (m_loading) return;
        int tag = static_cast<CCNode*>(sender)->getTag();
        m_selected = tag;
        this->updateSelection();
        this->bounceItem(tag);
    }

    void onPickLen(CCObject* sender) {
        if (m_loading) return;
        int tag = static_cast<CCNode*>(sender)->getTag();
        m_selectedLen = (m_selectedLen == tag) ? -1 : tag;
        this->updateSelection();
        this->bounceItem(6 + tag);
    }

    // ----- hover and click animations -----
    void animateItemTo(CCNode* node, float scale) {
        node->stopActionByTag(77);
        auto act = CCEaseSineOut::create(CCScaleTo::create(0.12f, scale));
        act->setTag(77);
        node->runAction(act);
    }

    void bounceItem(int idx) {
        auto& it = m_items[idx];
        it.visual->stopActionByTag(77);
        auto seq = CCSequence::create(
            CCEaseSineOut::create(CCScaleTo::create(0.07f, it.base * 1.4f)),
            CCEaseBackOut::create(CCScaleTo::create(0.25f, it.base * 1.15f)),
            nullptr
        );
        seq->setTag(77);
        it.visual->runAction(seq);
    }

    void hoverTick(float) {
        auto mouse = geode::cocos::getMousePos();
        for (auto& it : m_items) {
            auto parent = it.btn->getParent();
            if (!parent) continue;
            auto local = parent->convertToNodeSpace(mouse);
            bool inside = it.btn->boundingBox().containsPoint(local);
            if (inside != it.hovered) {
                it.hovered = inside;
                this->animateItemTo(it.visual, inside ? it.base * 1.15f : it.base);
            }
        }
    }

    // ----- wheel: keeps faces upright, flicks the pointer and ticks -----
    void wheelTick(float) {
        float rot = std::fmod(m_wheel->getRotation(), 360.f);
        if (rot < 0.f) rot += 360.f;

        for (auto f : m_wheelFaces) {
            f->setRotation(-rot);
        }

        int seg = static_cast<int>((rot + 30.f) / 60.f) % 6;
        if (seg != m_lastSeg) {
            m_lastSeg = seg;
            if (m_loading) {
                playSfx("counter003.ogg", 0.5f, 1.2f);
                m_pointer->stopAllActions();
                m_pointer->setRotation(0.f);
                m_pointer->runAction(CCSequence::create(
                    CCRotateTo::create(0.03f, -28.f),
                    CCRotateTo::create(0.07f, 0.f),
                    nullptr
                ));
            }
        }
    }

    // ----- spin button animations -----
    void pulseSpinButton() {
        m_spinSpr->stopAllActions();
        m_spinSpr->setRotation(0.f);
        m_spinSpr->setScale(1.f);
        m_spinSpr->runAction(CCRepeatForever::create(CCSequence::create(
            CCEaseSineInOut::create(CCScaleTo::create(0.6f, 1.08f)),
            CCEaseSineInOut::create(CCScaleTo::create(0.6f, 1.f)),
            nullptr
        )));
    }

    void wobbleSpinButton() {
        m_spinSpr->stopAllActions();
        m_spinSpr->setScale(0.9f);
        m_spinSpr->runAction(CCRepeatForever::create(CCSequence::create(
            CCRotateTo::create(0.08f, -4.f),
            CCRotateTo::create(0.08f, 4.f),
            nullptr
        )));
    }

    // ----- wheel animations -----
    void startIdleAnimation() {
        m_wheel->stopAllActions();
        m_wheel->runAction(CCScaleTo::create(0.2f, 1.f));
        m_wheel->runAction(CCRepeatForever::create(CCRotateBy::create(14.f, 360.f)));
    }

    void startSpinAnimation() {
        m_wheel->stopAllActions();
        // Squish, then pop bigger and whip around fast
        m_wheel->runAction(CCSequence::create(
            CCEaseSineOut::create(CCScaleTo::create(0.1f, 0.88f)),
            CCEaseBackOut::create(CCScaleTo::create(0.25f, 1.06f)),
            nullptr
        ));
        m_wheel->runAction(CCRepeatForever::create(CCRotateBy::create(0.35f, 360.f)));
    }

    void playReveal() {
        // Slow down like a real roulette wheel and land on the chosen difficulty
        m_wheel->stopAllActions();

        float cur = std::fmod(m_wheel->getRotation(), 360.f);
        if (cur < 0.f) cur += 360.f;
        float target = std::fmod(360.f - 60.f * m_selected, 360.f);
        float delta = 360.f * 5.f + std::fmod(target - cur + 360.f, 360.f);

        m_wheel->runAction(CCScaleTo::create(0.3f, 1.f));
        m_wheel->runAction(CCSequence::create(
            CCEaseExponentialOut::create(CCRotateBy::create(3.6f, delta)),
            CCCallFunc::create(this, callfunc_selector(RoulettePopup::showResult)),
            nullptr
        ));
    }

    void showResult() {
        // Win sound
        playSfx("achievement_01.ogg", 0.9f);

        // The winning face pops
        auto win = m_wheelFaces[m_selected];
        win->runAction(CCSequence::create(
            CCEaseBackOut::create(CCScaleTo::create(0.25f, 0.85f)),
            CCScaleTo::create(0.2f, 0.6f),
            nullptr
        ));
        m_wheel->runAction(CCSequence::create(
            CCScaleTo::create(0.12f, 1.12f),
            CCEaseBackOut::create(CCScaleTo::create(0.3f, 1.03f)),
            nullptr
        ));
        m_rim->runAction(CCSequence::create(
            CCScaleTo::create(0.12f, 1.1f),
            CCEaseBackOut::create(CCScaleTo::create(0.3f, 1.f)),
            nullptr
        ));

        // Screen shake (can be turned off in settings)
        if (Mod::get()->getSettingValue<bool>("screen-shake")) {
            m_mainLayer->runAction(CCSequence::create(
                CCMoveBy::create(0.04f, { 6.f, 0.f }),
                CCMoveBy::create(0.04f, { -12.f, 0.f }),
                CCMoveBy::create(0.04f, { 12.f, 0.f }),
                CCMoveBy::create(0.04f, { -6.f, 0.f }),
                nullptr
            ));
        }

        // White flash (can be turned off in settings)
        if (Mod::get()->getSettingValue<bool>("white-flash")) {
            m_flash->stopAllActions();
            m_flash->runAction(CCSequence::create(
                CCFadeTo::create(0.05f, 170),
                CCFadeTo::create(0.5f, 0),
                nullptr
            ));
        }

        // Burst of mini faces
        for (int i = 0; i < static_cast<int>(m_burst.size()); i++) {
            auto s = m_burst[i];
            float a = i * (kPi * 2.f / m_burst.size());
            float dist = 70.f + (i % 3) * 14.f;
            s->stopAllActions();
            s->setPosition({ m_size.width / 2, kWheelY });
            s->setOpacity(255);
            s->runAction(CCSpawn::create(
                CCEaseSineOut::create(CCMoveTo::create(0.7f, {
                    m_size.width / 2 + std::cos(a) * dist,
                    kWheelY + std::sin(a) * dist
                })),
                CCFadeOut::create(0.7f),
                nullptr
            ));
        }

        if (m_pendingLevel) {
            m_resultLabel->setString(fmt::format(
                "{} by {}",
                m_pendingLevel->m_levelName.c_str(),
                m_pendingLevel->m_creatorName.c_str()
            ).c_str());
        }
        m_resultLabel->limitLabelWidth(300.f, 0.5f, 0.2f);
        m_resultLabel->setOpacity(0);
        m_resultLabel->runAction(CCFadeIn::create(0.4f));

        if (Mod::get()->getSettingValue<bool>("auto-open")) {
            // Go straight to the level
            this->runAction(CCSequence::create(
                CCDelayTime::create(1.6f),
                CCCallFunc::create(this, callfunc_selector(RoulettePopup::onOpenAuto)),
                nullptr
            ));
        } else {
            // Show Open / Reroll after a short moment
            this->runAction(CCSequence::create(
                CCDelayTime::create(0.9f),
                CCCallFunc::create(this, callfunc_selector(RoulettePopup::showChoices)),
                nullptr
            ));
        }
    }

    // ----- open / reroll -----
    void showChoices() {
        m_loading = false;
        m_spinBtn->setVisible(false);
        m_choiceMenu->setVisible(true);
        for (auto b : { m_openBtn, m_rerollBtn }) {
            b->setScale(0.f);
            b->runAction(CCEaseBackOut::create(CCScaleTo::create(0.35f, 1.f)));
        }
    }

    void hideChoices() {
        m_choiceMenu->setVisible(false);
        m_spinBtn->setVisible(true);
        this->pulseSpinButton();
    }

    void onOpenAuto() {
        this->openLevel();
    }

    void onOpen(CCObject*) {
        this->openLevel();
    }

    void onReroll(CCObject*) {
        this->hideChoices();
        this->onSpin(nullptr);
    }

    // ----- searching -----
    void onSpin(CCObject*) {
        if (m_loading) return;
        m_loading = true;
        playSfx("playSound_01.ogg", 0.8f, 0.8f);
        this->wobbleSpinButton();
        m_maxPage = 19;
        m_resultLabel->setOpacity(255);
        m_resultLabel->setString("Searching...");
        m_resultLabel->limitLabelWidth(300.f, 0.5f, 0.2f);
        this->startSpinAnimation();
        this->startSearch();
    }

    void startSearch() {
        std::uniform_int_distribution<int> pagePick(0, m_maxPage);
        auto search = GJSearchObject::create(SearchType::Search, "");
        search->m_difficulty = "-2";
        search->m_uncompletedFilter = true;

        if (m_selectedLen >= 0) {
            // +1 because Tiny (length 0) was removed from the buttons
            search->m_length = std::to_string(m_selectedLen + 1);
        }

        // Filters from the Filters popup
        if (Mod::get()->getSavedValue<bool>("filter-rated")) search->m_starFilter = true;
        if (Mod::get()->getSavedValue<bool>("filter-featured")) search->m_featuredFilter = true;
        if (Mod::get()->getSavedValue<bool>("filter-epic")) search->m_epicFilter = true;

        switch (m_selected) {
            case 1: search->m_demonFilter = GJDifficulty::DemonEasy; break;
            case 2: search->m_demonFilter = GJDifficulty::DemonMedium; break;
            case 3: search->m_demonFilter = GJDifficulty::Demon; break;
            case 4: search->m_demonFilter = GJDifficulty::DemonInsane; break;
            case 5: search->m_demonFilter = GJDifficulty::DemonExtreme; break;
            default: break;
        }

        search = search->getPageObject(pagePick(m_rng));

        auto glm = GameLevelManager::get();
        glm->m_levelManagerDelegate = this;
        glm->getOnlineLevels(search);
    }

    // True if you have any attempts or any percentage saved on this level
    static bool hasProgress(GJGameLevel* lvl) {
        return lvl->getNormalPercent() > 0
            || lvl->m_practicePercent > 0
            || lvl->m_attempts.value() > 0;
    }

    void loadLevelsFinished(CCArray* levels, char const*) override {
        if (!levels || levels->count() == 0) {
            this->retryOrFail();
            return;
        }

        // Keep only levels you have never completed, played, opened or been given before
        auto seen = Mod::get()->getSavedValue<std::vector<int>>("seen-levels");
        std::vector<GJGameLevel*> fresh;
        for (unsigned int i = 0; i < levels->count(); i++) {
            auto lvl = static_cast<GJGameLevel*>(levels->objectAtIndex(i));
            int id = lvl->m_levelID.value();
            if (std::find(seen.begin(), seen.end(), id) != seen.end()) continue;
            if (GameStatsManager::get()->hasCompletedOnlineLevel(id)) continue;
            if (GameLevelManager::get()->hasDownloadedLevel(id)) continue;
            if (hasProgress(lvl)) continue;
            auto saved = GameLevelManager::get()->getSavedLevel(id);
            if (saved && hasProgress(saved)) continue;
            fresh.push_back(lvl);
        }

        if (fresh.empty()) {
            this->retryOrFail();
            return;
        }

        std::uniform_int_distribution<size_t> pick(0, fresh.size() - 1);
        m_pendingLevel = fresh[pick(m_rng)];

        // Remember this level so it is never given again
        seen.push_back(m_pendingLevel->m_levelID.value());
        if (seen.size() > 5000) seen.erase(seen.begin());
        Mod::get()->setSavedValue("seen-levels", seen);

        recordSpin();
        this->playReveal();
    }

    void loadLevelsFailed(char const*) override {
        this->retryOrFail();
    }

    void setupPageInfo(gd::string, char const*) override {}

    void retryOrFail() {
        if (m_maxPage > 0) {
            m_maxPage /= 2;
            this->startSearch();
            return;
        }
        m_loading = false;
        this->pulseSpinButton();
        this->startIdleAnimation();
        m_resultLabel->setString("Nothing new found, spin again!");
        m_resultLabel->limitLabelWidth(300.f, 0.5f, 0.2f);
    }

    // Saves the level you opened into the History list
    void addToHistory() {
        if (!m_pendingLevel) return;
        auto ids = Mod::get()->getSavedValue<std::vector<int>>("history-ids");
        auto names = Mod::get()->getSavedValue<std::vector<std::string>>("history-names");
        if (ids.size() != names.size()) {
            ids.clear();
            names.clear();
        }
        ids.push_back(m_pendingLevel->m_levelID.value());
        names.push_back(fmt::format(
            "{} by {}",
            m_pendingLevel->m_levelName.c_str(),
            m_pendingLevel->m_creatorName.c_str()
        ));
        while (ids.size() > 30) {
            ids.erase(ids.begin());
            names.erase(names.begin());
        }
        Mod::get()->setSavedValue("history-ids", ids);
        Mod::get()->setSavedValue("history-names", names);
    }

    void openLevel() {
        if (m_pendingLevel) {
            this->addToHistory();
            recordOpen();
            this->updateStreakLabel();
            auto scene = LevelInfoLayer::scene(m_pendingLevel, false);
            CCDirector::get()->pushScene(CCTransitionFade::create(0.5f, scene));
        }
        m_loading = false;
        this->hideChoices();
        this->startIdleAnimation();
    }

public:
    ~RoulettePopup() {
        auto glm = GameLevelManager::get();
        if (glm->m_levelManagerDelegate == this) {
            glm->m_levelManagerDelegate = nullptr;
        }
    }

    static RoulettePopup* create() {
        auto ret = new RoulettePopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class $modify(MyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto spr = CircleButtonSprite::createWithSpriteFrameName(
            "diffIcon_06_btn_001.png",
            1.0f,
            CircleBaseColor::Green,
            CircleBaseSize::Medium
        );
        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(MyMenuLayer::onRoulette)
        );
        btn->setID("roulette-button"_spr);

        if (auto menu = this->getChildByID("bottom-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        }
        return true;
    }

    void onRoulette(CCObject*) {
        RoulettePopup::create()->show();
    }
};
