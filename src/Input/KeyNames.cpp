#include "Input/KeyNames.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cctype>

std::string keyName(int key) {
    switch (key) {
        case GLFW_KEY_SPACE:         return "Space";
        case GLFW_KEY_LEFT_SHIFT:    return "L Shift";
        case GLFW_KEY_RIGHT_SHIFT:   return "R Shift";
        case GLFW_KEY_LEFT_CONTROL:  return "L Ctrl";
        case GLFW_KEY_RIGHT_CONTROL: return "R Ctrl";
        case GLFW_KEY_LEFT_ALT:      return "L Alt";
        case GLFW_KEY_RIGHT_ALT:     return "R Alt";
        case GLFW_KEY_TAB:           return "Tab";
        case GLFW_KEY_CAPS_LOCK:     return "Caps Lock";
        case GLFW_KEY_ENTER:         return "Enter";
        case GLFW_KEY_BACKSPACE:     return "Backspace";
        case GLFW_KEY_UP:            return "Up";
        case GLFW_KEY_DOWN:          return "Down";
        case GLFW_KEY_LEFT:          return "Left";
        case GLFW_KEY_RIGHT:         return "Right";
        case GLFW_KEY_INSERT:        return "Insert";
        case GLFW_KEY_DELETE:        return "Delete";
        case GLFW_KEY_HOME:          return "Home";
        case GLFW_KEY_END:           return "End";
        case GLFW_KEY_PAGE_UP:       return "Page Up";
        case GLFW_KEY_PAGE_DOWN:     return "Page Down";
        default: break;
    }
    if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25) return "F" + std::to_string(key - GLFW_KEY_F1 + 1);
    if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9) return "Num " + std::to_string(key - GLFW_KEY_KP_0);

    // GLFW codes for letters and digits are their US-layout ASCII characters; the
    // font is ASCII-only, so non-Latin layouts would otherwise render nothing.
    if ((key >= GLFW_KEY_A && key <= GLFW_KEY_Z) || (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)) {
        return std::string(1, static_cast<char>(key));
    }
    if (const char* name = glfwGetKeyName(key, 0)) {
        std::string s(name);
        const bool ascii = !s.empty() && std::all_of(s.begin(), s.end(),
            [](char c) { return static_cast<unsigned char>(c) < 128; });
        if (ascii) {
            for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return s;
        }
    }
    return "Key " + std::to_string(key);
}
