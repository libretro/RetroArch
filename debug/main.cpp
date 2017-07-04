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

bool show_test_window = true;

ImVec4 clear_color;

SDL_Window* debugger_window;
SDL_GLContext glcontext;

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
   SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
   SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
   SDL_DisplayMode current;
   SDL_GetCurrentDisplayMode(0, &current);
   debugger_window = SDL_CreateWindow("ImGui SDL2+OpenGL example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
   SDL_GLContext glcontext = SDL_GL_CreateContext(debugger_window);

   // Setup ImGui binding
   ImGui_ImplSdl_Init(debugger_window);

   // Load Fonts
   // (there is a default font, this is only if you want to change it. see extra_fonts/README.txt for more details)
   //ImGuiIO& io = ImGui::GetIO();
   //io.Fonts->AddFontDefault();
   //io.Fonts->AddFontFromFileTTF("../../extra_fonts/Cousine-Regular.ttf", 15.0f);
   //io.Fonts->AddFontFromFileTTF("../../extra_fonts/DroidSans.ttf", 16.0f);
   //io.Fonts->AddFontFromFileTTF("../../extra_fonts/ProggyClean.ttf", 13.0f);
   //io.Fonts->AddFontFromFileTTF("../../extra_fonts/ProggyTiny.ttf", 10.0f);
   //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
   clear_color = ImColor(114, 144, 154);

}

void debugger_draw(bool* deinit)
{

   while (*deinit == false)
   {
      SDL_Event event;
      while (SDL_PollEvent(&event))
      {
          ImGui_ImplSdl_ProcessEvent(&event);
      }
      ImGui_ImplSdl_NewFrame(debugger_window);

      // 1. Show a simple window
      // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
      {
          static float f = 0.0f;
          ImGui::Text("Hello, world!");
          ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
          ImGui::ColorEdit3("clear color", (float*)&clear_color);
          if (ImGui::Button("Test Window")) show_test_window ^= 1;
          ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
      }

      if (show_test_window)
      {
          ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver);
          ImGui::ShowTestWindow(&show_test_window);
      }

      // Rendering
      glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
      glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
      glClear(GL_COLOR_BUFFER_BIT);
      ImGui::Render();
      SDL_GL_SwapWindow(debugger_window);
  }

   // Cleanup
   ImGui_ImplSdl_Shutdown();
   SDL_GL_DeleteContext(glcontext);
   SDL_DestroyWindow(debugger_window);
   debugger_window = NULL;
   return;
}


