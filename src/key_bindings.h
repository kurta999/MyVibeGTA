#pragma once
#include <array>
#include <cstddef>

// Validation for user-remappable keys. Kept header-only so the rules can be
// unit-tested without a window or a settings file.
namespace key_bindings {
// Keys the game reserves for menus, system shortcuts or fixed actions.
// Mouse buttons and modifiers other than Shift/Ctrl are rejected as well.
inline bool reserved(int key) {
    switch (key) {
    case 0x01:
    case 0x02:
    case 0x04:
    case 0x05:
    case 0x06: // mouse buttons
    case 0x1B: // Escape (pause / cancel)
    case 0x12:
    case 0xA4:
    case 0xA5: // Alt (system menu)
    case 0x5B:
    case 0x5C:
    case 0x5D: // Windows / application keys
    case 0x70:
    case 0x72:
    case 0x73:
    case 0x74:
    case 0x7A: // F1 F3 F4 F5 F11
        return true;
    default:
        return false;
    }
}
inline bool valid(int key) {
    return key > 0 && key < 255 && !reserved(key);
}

// Replaces invalid entries with their defaults and resolves duplicates so
// every action keeps a distinct key. Returns true when anything changed.
template <std::size_t N>
bool sanitize(std::array<int, N>& bindings, const std::array<int, N>& defaults) {
    bool changed = false;
    for (std::size_t i = 0; i < N; ++i)
        if (!valid(bindings[i])) {
            bindings[i] = defaults[i];
            changed = true;
        }
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t j = 0; j < i; ++j) {
            if (bindings[i] != bindings[j])
                continue;
            // Prefer the action's own default; otherwise any unused default.
            int replacement = defaults[i];
            auto used = [&](int key) {
                for (std::size_t k = 0; k < N; ++k)
                    if (k != i && bindings[k] == key)
                        return true;
                return false;
            };
            if (used(replacement))
                for (int candidate : defaults)
                    if (!used(candidate)) {
                        replacement = candidate;
                        break;
                    }
            if (!used(replacement)) {
                bindings[i] = replacement;
                changed = true;
            }
            break;
        }
    return changed;
}

// Assigns key to action. If another action already uses that key, the two
// actions swap so no binding is lost. Returns false for invalid keys.
template <std::size_t N> bool assign(std::array<int, N>& bindings, std::size_t action, int key) {
    if (action >= N || !valid(key))
        return false;
    for (std::size_t i = 0; i < N; ++i)
        if (i != action && bindings[i] == key) {
            bindings[i] = bindings[action];
            break;
        }
    bindings[action] = key;
    return true;
}
} // namespace key_bindings
