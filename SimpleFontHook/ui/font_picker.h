#pragma once
#include <windows.h>
#include <vector>
#include <string>

namespace FontPicker {
    // Initialize and create the picker window (call from a worker thread)
    void Init(HMODULE hModule);
    // Toggle visibility
    void Toggle();
    // Is picker currently visible?
    bool IsVisible();

}
