/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2017 - Andrés Suárez
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <SDL.h>
#include <SDL_opengl.h>

#include "main.h"
#include "pluginman.h"

#include "imguidock.h"
#include "imguial_fonts.h"

static SDL_Window*   s_debugger_window;
static SDL_GLContext s_debugger_context;

void debugger_init()
{
  // Setup SDL
  if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) != 0)
  {
    printf("Error: %s\n", SDL_GetError());
    return;
  }

  // Setup window
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

  SDL_DisplayMode current;
  SDL_GetCurrentDisplayMode(0, &current);

  s_debugger_window = SDL_CreateWindow("RetroArch Debugger", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
  s_debugger_context = SDL_GL_CreateContext(s_debugger_window);

  // Setup ImGui binding
  ImGui_ImplSdl_Init(s_debugger_window);

  // Load Fonts
  int ttf_size;
  const void* ttf_data = ImGuiAl::Fonts::GetCompressedData(ImGuiAl::Fonts::kProggyTiny, &ttf_size);

  ImGuiIO& io = ImGui::GetIO();
  ImFont* font = io.Fonts->AddFontFromMemoryCompressedTTF(ttf_data, ttf_size, 10.0f);
  font->DisplayOffset.y = 1.0f;

  ImFontConfig config;
  config.MergeMode = true;
  config.PixelSnapH = true;

  static const ImWchar ranges1[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
  ttf_data = ImGuiAl::Fonts::GetCompressedData(ImGuiAl::Fonts::kFontAwesome, &ttf_size);
  io.Fonts->AddFontFromMemoryCompressedTTF(ttf_data, ttf_size, 12.0f, &config, ranges1);

  debugger_pluginman_init();
}

void debugger_draw(volatile bool* deinit)
{
  while (*deinit == false)
  {
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
      ImGui_ImplSdl_ProcessEvent(&event);
    }

    ImGui_ImplSdl_NewFrame(s_debugger_window);

    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;

    const float oldWindowRounding = ImGui::GetStyle().WindowRounding;
    ImGui::GetStyle().WindowRounding = 0;
    const bool visible = ImGui::Begin("Docking Manager", NULL, ImVec2(0, 0), 1.0f, flags);
    ImGui::GetStyle().WindowRounding = oldWindowRounding;

    if (visible)
    {
      ImGui::BeginDockspace();
      debugger_pluginman_draw();
      ImGui::EndDockspace();
    }

    ImGui::End();

    // Rendering
    glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
    glClearColor(0.05f, 0.05f, 0.05f, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui::Render();
    SDL_GL_SwapWindow(s_debugger_window);
  }

  // Cleanup
  debugger_pluginman_deinit();
  ImGui_ImplSdl_Shutdown();
  SDL_GL_DeleteContext(s_debugger_context);
  SDL_DestroyWindow(s_debugger_window);
}


