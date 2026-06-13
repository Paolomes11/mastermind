#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>
#include <implot.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "app_state.hpp"
#include "render.hpp"

// ── Font / scale helpers ──────────────────────────────────────────────────────

static void rebuild_fonts(float size_px) {
    // Destroy all GPU objects (includes font texture), rebuild font atlas, recreate.
    ImGui_ImplOpenGL3_DestroyDeviceObjects();

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
#ifdef IMGUI_FONT_PATH
    ImFont* f = io.Fonts->AddFontFromFileTTF(IMGUI_FONT_PATH, size_px);
    if (!f)
        io.Fonts->AddFontDefault();
#else
    io.Fonts->AddFontDefault();
    (void)size_px;
#endif
    io.Fonts->Build();

    ImGui_ImplOpenGL3_CreateDeviceObjects();
}

// Returns a UI scale factor relative to the 1280×720 baseline.
static float compute_scale(int w, int h) {
    float sx = w / 1280.0f;
    float sy = h / 720.0f;
    return std::max(0.5f, std::min(sx, sy));  // clamp: never below 0.5×
}

static void apply_scale(float scale, const ImGuiStyle& base_style) {
    ImGui::GetStyle() = base_style;
    ImGui::GetStyle().ScaleAllSizes(scale);
    rebuild_fonts(std::round(22.0f * scale));
}

static void do_toggle_fullscreen(SDL_Window* win, AppState& state, const ImGuiStyle& base_style) {
    bool currently_full = (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
    SDL_SetWindowFullscreen(win, currently_full ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
    state.is_fullscreen = !currently_full;
    int w, h;
    SDL_GL_GetDrawableSize(win, &w, &h);
    apply_scale(compute_scale(w, h), base_style);
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    // OpenGL 3.3 core profile
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_WindowFlags wflags = static_cast<SDL_WindowFlags>(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                                          SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Mastermind Entropy Solver", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 1280, 720, wflags);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);
    if (!gl_ctx) {
        fprintf(stderr, "SDL_GL_CreateContext error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, gl_ctx);
    SDL_GL_SetSwapInterval(1);  // vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 3.0f;
    style.WindowBorderSize = 0.0f;
    style.FramePadding = {6.0f, 4.0f};
    style.ItemSpacing = {8.0f, 6.0f};

    // Save the base style (at scale 1.0) for later rescaling
    const ImGuiStyle base_style = style;

    ImGui_ImplSDL2_InitForOpenGL(window, gl_ctx);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Load font at baseline scale (1280×720 = scale 1.0 → 16 px)
    apply_scale(1.0f, base_style);

    AppState state;
    state.reset();

    bool quit = false;
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_QUIT) {
                quit = true;
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_CLOSE &&
                    event.window.windowID == SDL_GetWindowID(window)) {
                    quit = true;
                } else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    int w, h;
                    SDL_GL_GetDrawableSize(window, &w, &h);
                    apply_scale(compute_scale(w, h), base_style);
                }
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) {
                do_toggle_fullscreen(window, state, base_style);
            }
        }

        // Handle fullscreen toggle requested from the render layer (button click)
        if (state.request_fullscreen_toggle) {
            state.request_fullscreen_toggle = false;
            do_toggle_fullscreen(window, state, base_style);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        double time_s = static_cast<double>(SDL_GetTicks64()) / 1000.0;
        render_frame(state, time_s);

        ImGui::Render();

        int w, h;
        SDL_GL_GetDrawableSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.11f, 0.11f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Ensure background task finishes before destroying state
    if (state.score_future.valid())
        state.score_future.get();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
