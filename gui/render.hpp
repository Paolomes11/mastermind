#pragma once
#include "app_state.hpp"

// Main render entry point — called once per frame from the event loop.
// current_time_s is SDL_GetTicks64() / 1000.0
void render_frame(AppState& state, double current_time_s);
