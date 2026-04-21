# Конфігурація

## Огляд

`Config` клас управляє завантаженням та збереженням налаштувань гри в форматі JSON. Підтримує графіку, управління та аудіо параметри.

## API

```cpp
#pragma once
#include <string>

class Config {
public:
    Config() = default;
    ~Config() = default;

    // Завантаження/збереження
    bool load(const std::string& filename = "config.json");
    bool save(const std::string& filename = "config.json") const;

    // === Graphics ===
    int getWindowWidth() const { return m_windowWidth; }
    int getWindowHeight() const { return m_windowHeight; }
    float getFOV() const { return m_fov; }
    int getRenderDistance() const { return m_renderDistance; }
    bool getVSync() const { return m_vsync; }
    bool getFullscreen() const { return m_fullscreen; }

    void setWindowWidth(int width) { m_windowWidth = width; }
    void setWindowHeight(int height) { m_windowHeight = height; }
    void setFOV(float fov) { m_fov = fov; }
    void setRenderDistance(int distance) { m_renderDistance = distance; }
    void setVSync(bool vsync) { m_vsync = vsync; }
    void setFullscreen(bool fullscreen) { m_fullscreen = fullscreen; }

    // === Controls ===
    float getMouseSensitivity() const { return m_mouseSensitivity; }
    float getMovementSpeed() const { return m_movementSpeed; }

    void setMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }
    void setMovementSpeed(float speed) { m_movementSpeed = speed; }

    // === Audio ===
    float getMasterVolume() const { return m_masterVolume; }
    float getMusicVolume() const { return m_musicVolume; }
    float getSfxVolume() const { return m_sfxVolume; }

    void setMasterVolume(float volume) { m_masterVolume = volume; }
    void setMusicVolume(float volume) { m_musicVolume = volume; }
    void setSfxVolume(float volume) { m_sfxVolume = volume; }

private:
    // Graphics
    int m_windowWidth = 800;
    int m_windowHeight = 600;
    float m_fov = 70.0f;
    int m_renderDistance = 8;
    bool m_vsync = true;
    bool m_fullscreen = false;

    // Controls
    float m_mouseSensitivity = 0.1f;
    float m_movementSpeed = 10.0f;

    // Audio
    float m_masterVolume = 1.0f;
    float m_musicVolume = 0.7f;
    float m_sfxVolume = 0.8f;
};
```

## Формат config.json

```json
{
  "graphics": {
    "windowWidth": 800,
    "windowHeight": 600,
    "fov": 70.0,
    "renderDistance": 8,
    "vsync": true,
    "fullscreen": false
  },
  "controls": {
    "mouseSensitivity": 0.1,
    "movementSpeed": 10.0
  },
  "audio": {
    "masterVolume": 1.0,
    "musicVolume": 0.7,
    "sfxVolume": 0.8
  },
  "ui": {
    "font": "fonts/default.ttf",
    "fontSize": 16
  }
}
```

## Завантаження

```cpp
bool Config::load(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "[Config] File not found, using defaults" << std::endl;
        return false;
    }

    try {
        nlohmann::json json;
        file >> json;

        // Graphics
        if (json.contains("graphics")) {
            auto& graphics = json["graphics"];
            m_windowWidth = graphics.value("windowWidth", m_windowWidth);
            m_windowHeight = graphics.value("windowHeight", m_windowHeight);
            m_fov = graphics.value("fov", m_fov);
            m_renderDistance = graphics.value("renderDistance", m_renderDistance);
            m_vsync = graphics.value("vsync", m_vsync);
            m_fullscreen = graphics.value("fullscreen", m_fullscreen);
        }

        // Controls
        if (json.contains("controls")) {
            auto& controls = json["controls"];
            m_mouseSensitivity = controls.value("mouseSensitivity", m_mouseSensitivity);
            m_movementSpeed = controls.value("movementSpeed", m_movementSpeed);
        }

        // Audio
        if (json.contains("audio")) {
            auto& audio = json["audio"];
            m_masterVolume = audio.value("masterVolume", m_masterVolume);
            m_musicVolume = audio.value("musicVolume", m_musicVolume);
            m_sfxVolume = audio.value("sfxVolume", m_sfxVolume);
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Config] Error loading: " << e.what() << std::endl;
        return false;
    }
}
```

## Збереження

```cpp
bool Config::save(const std::string& filename) const {
    nlohmann::json json;

    json["graphics"] = {
        {"windowWidth", m_windowWidth},
        {"windowHeight", m_windowHeight},
        {"fov", m_fov},
        {"renderDistance", m_renderDistance},
        {"vsync", m_vsync},
        {"fullscreen", m_fullscreen}
    };

    json["controls"] = {
        {"mouseSensitivity", m_mouseSensitivity},
        {"movementSpeed", m_movementSpeed}
    };

    json["audio"] = {
        {"masterVolume", m_masterVolume},
        {"musicVolume", m_musicVolume},
        {"sfxVolume", m_sfxVolume}
    };

    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    file << json.dump(2);  // 2 spaces indent
    return true;
}
```

## Значення за замовчуванням

| Параметр | Значення | Діапазон |
|----------|----------|----------|
| windowWidth | 800 | > 0 |
| windowHeight | 600 | > 0 |
| fov | 70.0 | 30-120 |
| renderDistance | 8 | 1-32 |
| vsync | true | true/false |
| fullscreen | false | true/false |
| mouseSensitivity | 0.1 | 0.001-1.0 |
| movementSpeed | 10.0 | 1.0-100.0 |
| masterVolume | 1.0 | 0.0-1.0 |

## Використання в коді

```cpp
// Engine::init()
Config config;
config.load("config.json");

// Застосовуємо налаштування
windowWidth = config.getWindowWidth();
windowHeight = config.getWindowHeight();

// В SettingsState — зміна та збереження
void SettingsState::onExit() {
    m_config->save();  // Зберігаємо при виході
}

void SettingsState::handleInput() {
    if (input.isKeyPressed(GLFW_KEY_LEFT)) {
        m_config->setFOV(std::max(30.0f, m_config->getFOV() - 5.0f));
    }
    if (input.isKeyPressed(GLFW_KEY_RIGHT)) {
        m_config->setFOV(std::min(120.0f, m_config->getFOV() + 5.0f));
    }
}
```

## Дивіться також

- [[states/SettingsState\|SettingsState]] — зміна налаштувань
- [[Engine\|Engine]] — застосування налаштувань
