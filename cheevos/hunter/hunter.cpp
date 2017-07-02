#include <stdio.h>
#include "imgui.h"
#include "imgui_impl_glfw_gl3.h"
#include "gl3w.h"

#include "hunter.h"

bool show_test_window = true;

ImVec4 clear_color;

GLFWwindow* hunter_window;
int frame;

static void error_callback(int error, const char* description)
{
   printf("Error %d: %s\n", error, description);
}

void hunter_init()
{

   glfwSetErrorCallback(error_callback);
   if (!glfwInit())
      return;
   glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
   glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
   glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if __APPLE__
   glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
   hunter_window = glfwCreateWindow(1280, 720, "ImGui OpenGL3 example", NULL, NULL);
   glfwMakeContextCurrent(hunter_window);
   gl3wInit();

   ImGui_ImplGlfwGL3_Init(hunter_window, true);
   clear_color = ImColor(114, 144, 154);
}

void hunter_draw(bool* deinit)
{

   while (*deinit == false)
   {
      glfwPollEvents();
      ImGui_ImplGlfwGL3_NewFrame();

      {
         ImGui::Text("Hello, world!");
         ImGui::ColorEdit3("clear color", (float*)&clear_color);
         if (ImGui::Button("Test Window")) show_test_window ^= 1;
         ImGui::Text("frames %d", frame);
         frame ++;
        }

        if (show_test_window)
        {
            ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver);
            ImGui::ShowTestWindow(&show_test_window);
        }

         int display_w, display_h;
         glfwGetFramebufferSize(hunter_window, &display_w, &display_h);
         glViewport(0, 0, display_w, display_h);
         glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
         glClear(GL_COLOR_BUFFER_BIT);
         ImGui::Render();
         glfwSwapBuffers(hunter_window);
   }

   ImGui_ImplGlfwGL3_Shutdown();
   glfwTerminate();
   hunter_window = NULL;
   return;
}


