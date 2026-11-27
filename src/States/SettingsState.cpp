#include "States/SettingsState.hpp"
#include "Input/KeyNames.hpp"
#include "World/Chunk.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace {
constexpr int kFpsOptions[] = {0, 30, 60, 90, 120, 144, 165, 240};  // 0 = unlimited
constexpr int kSpeedOptions[] = {1, 2, 5, 10, 20, 50, 100, 200};    // m/s
constexpr const char* kActionLabels[Config::KEY_ACTION_COUNT] = {
    "Forward", "Back", "Left", "Right", "Up", "Down"};

const glm::vec4 kButtonNormal (0.25f, 0.27f, 0.33f, 1.0f);
const glm::vec4 kButtonHover  (0.36f, 0.39f, 0.48f, 1.0f);
const glm::vec4 kButtonPressed(0.16f, 0.17f, 0.22f, 1.0f);

constexpr float kRowTop    = 0.30f;
constexpr float kRowStep   = 0.085f;
constexpr float kRowHeight = 0.065f;

// Steps through a sorted option list; a hand-edited value snaps to the nearest option first.
template <std::size_t N>
int stepOption(const int (&options)[N], float current, int delta) {
    std::size_t index = 0;
    for (std::size_t i = 1; i < N; ++i) {
        if (std::abs(options[i] - current) < std::abs(options[index] - current)) index = i;
    }
    const int next = std::clamp(static_cast<int>(index) + delta, 0, static_cast<int>(N) - 1);
    return options[next];
}

std::unique_ptr<UIButton> makeButton(float x, float y, float w, const std::string& text) {
    return std::make_unique<UIButton>(glm::vec2(x, y), glm::vec2(w, kRowHeight), text,
                                      kButtonNormal, kButtonHover, kButtonPressed);
}

std::unique_ptr<UILabel> makeLabel(float x, float y, float w, const std::string& text,
                                   float h = kRowHeight) {
    return std::make_unique<UILabel>(glm::vec2(x, y), glm::vec2(w, h), text, 16.0f,
                                     glm::vec4(1.0f));
}
}

SettingsState::SettingsState(Engine* engine)
    : m_engine(engine) {
    createUI();
}

void SettingsState::createUI() {
    m_panel = std::make_unique<UIPanel>(glm::vec2(0.0f), glm::vec2(1.0f),
                                        glm::vec4(0.0f, 0.0f, 0.0f, 0.6f));

    auto title = makeLabel(0.35f, 0.06f, 0.30f, "SETTINGS", 0.10f);
    title->setCentered(true);
    m_panel->addChild(std::move(title));

    // Graphics column.
    const float gx = 0.06f;
    m_panel->addChild(makeLabel(gx, kRowTop - kRowStep, 0.40f, "Graphics"));

    m_distanceValue = addStepper(gx, kRowTop, "View distance", [this](int d) { stepRenderDistance(d); });
    m_fpsValue = addStepper(gx, kRowTop + kRowStep, "FPS limit", [this](int d) { stepFpsLimit(d); });

    const float vsyncY = kRowTop + 2.0f * kRowStep;
    m_panel->addChild(makeLabel(gx, vsyncY, 0.18f, "VSync"));
    auto vsync = makeButton(gx + 0.19f, vsyncY, 0.22f, "");
    vsync->setOnClick([this]() {
        Config& config = m_engine->getConfig();
        config.setVsync(!config.isVsync());
        m_engine->getRenderer().setVsync(config.isVsync());
        config.save();
        refreshLabels();
    });
    m_vsyncButton = vsync.get();
    m_panel->addChild(std::move(vsync));

    const float moveY = kRowTop + 4.0f * kRowStep;
    m_panel->addChild(makeLabel(gx, moveY - kRowStep, 0.40f, "Movement"));
    m_speedValue = addStepper(gx, moveY, "Speed", [this](int d) { stepSpeed(d); });

    // Controls column.
    const float cx = 0.54f;
    m_panel->addChild(makeLabel(cx, kRowTop - kRowStep, 0.40f, "Controls"));
    for (std::size_t i = 0; i < Config::KEY_ACTION_COUNT; ++i) {
        const float y = kRowTop + static_cast<float>(i) * kRowStep;
        m_panel->addChild(makeLabel(cx, y, 0.16f, kActionLabels[i]));
        auto keyBtn = makeButton(cx + 0.17f, y, 0.23f, "");
        keyBtn->setOnClick([this, i]() {
            m_waitingAction = static_cast<int>(i);
            refreshLabels();
        });
        m_keyButtons[i] = keyBtn.get();
        m_panel->addChild(std::move(keyBtn));
    }

    auto back = std::make_unique<UIButton>(glm::vec2(0.40f, 0.86f), glm::vec2(0.20f, 0.08f), "Back",
        glm::vec4(0.70f, 0.25f, 0.25f, 1.0f),
        glm::vec4(0.90f, 0.35f, 0.35f, 1.0f),
        glm::vec4(0.50f, 0.15f, 0.15f, 1.0f));
    back->setOnClick([this]() { goBack(); });
    m_panel->addChild(std::move(back));
}

UILabel* SettingsState::addStepper(float x, float y, const std::string& name,
                                   std::function<void(int)> onStep) {
    m_panel->addChild(makeLabel(x, y, 0.18f, name));
    auto down = makeButton(x + 0.19f, y, 0.04f, "<");
    down->setOnClick([onStep]() { onStep(-1); });
    m_panel->addChild(std::move(down));
    auto value = makeLabel(x + 0.23f, y, 0.14f, "");
    value->setCentered(true);
    UILabel* valuePtr = value.get();
    m_panel->addChild(std::move(value));
    auto up = makeButton(x + 0.37f, y, 0.04f, ">");
    up->setOnClick([onStep]() { onStep(1); });
    m_panel->addChild(std::move(up));
    return valuePtr;
}

void SettingsState::refreshLabels() {
    const Config& config = m_engine->getConfig();

    const int distance = config.getRenderDistance();
    const int metres = static_cast<int>(std::lround(distance * CHUNK_SIZE * VOXEL_SIZE));
    m_distanceValue->setText(std::to_string(distance) + " (" + std::to_string(metres) + " m)");

    const int fps = config.getFpsLimit();
    m_fpsValue->setText(fps > 0 ? std::to_string(fps) : "Unlimited");

    m_vsyncButton->setText(config.isVsync() ? "On" : "Off");

    std::ostringstream speed;
    speed << std::setprecision(3) << config.getMovementSpeed() << " m/s";
    m_speedValue->setText(speed.str());

    for (std::size_t i = 0; i < Config::KEY_ACTION_COUNT; ++i) {
        m_keyButtons[i]->setText(static_cast<int>(i) == m_waitingAction
            ? "Press a key..."
            : keyName(config.getKey(static_cast<KeyAction>(i))));
    }
}

void SettingsState::stepRenderDistance(int delta) {
    Config& config = m_engine->getConfig();
    // Fine steps up to 16 chunks, coarser beyond; there is no upper limit.
    const int distance = config.getRenderDistance();
    const int step = (delta > 0 ? distance >= 16 : distance > 16) ? 4 : 1;
    config.setRenderDistance(distance + delta * step);
    config.save();
    refreshLabels();
}

void SettingsState::stepFpsLimit(int delta) {
    Config& config = m_engine->getConfig();
    config.setFpsLimit(stepOption(kFpsOptions, static_cast<float>(config.getFpsLimit()), delta));
    config.save();
    refreshLabels();
}

void SettingsState::stepSpeed(int delta) {
    Config& config = m_engine->getConfig();
    config.setMovementSpeed(static_cast<float>(stepOption(kSpeedOptions, config.getMovementSpeed(), delta)));
    m_engine->getCamera().setMovementSpeed(config.getMovementSpeed());
    config.save();
    refreshLabels();
}

void SettingsState::bindKey(int key) {
    Config& config = m_engine->getConfig();
    const auto action = static_cast<KeyAction>(m_waitingAction);

    // Swap with any action already using this key so no binding is lost.
    for (std::size_t i = 0; i < Config::KEY_ACTION_COUNT; ++i) {
        const auto other = static_cast<KeyAction>(i);
        if (other != action && config.getKey(other) == key) {
            config.setKey(other, config.getKey(action));
        }
    }
    config.setKey(action, key);
    config.save();

    m_waitingAction = -1;
    refreshLabels();
}

void SettingsState::goBack() {
    m_waitingAction = -1;
    m_nextState = m_returnState;
    m_shouldChangeState = true;
}

void SettingsState::onEnter() {
    m_shouldChangeState = false;
    m_nextState = GameState::SETTINGS;
    m_returnState = m_engine->getStateManager().getPreviousState();
    m_waitingAction = -1;
    m_engine->getInput().setCursorEnabled(true);
    refreshLabels();
}

void SettingsState::onExit() {
}

void SettingsState::update(float deltaTime) {
    m_panel->update(deltaTime);

    auto& input = m_engine->getInput();
    if (m_waitingAction >= 0) {
        const int key = input.getLastKeyPressed();
        if (key == GLFW_KEY_ESCAPE) {
            m_waitingAction = -1;
            refreshLabels();
        } else if (key != GLFW_KEY_UNKNOWN) {
            bindKey(key);
        }
    } else if (input.isKeyPressed(GLFW_KEY_ESCAPE)) {
        goBack();
    }
}

void SettingsState::handleInput() {
    auto& input = m_engine->getInput();
    m_panel->handleInput(input.getNormalizedMousePosition(),
                         input.isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT));
}
