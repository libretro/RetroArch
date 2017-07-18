/*
The MIT License (MIT)

Copyright (c) 2016 Andre Leiradella

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

namespace ImGuiAl
{
  inline bool Button( const char* label, bool enabled = true, const ImVec2& size = ImVec2( 0, 0 ) )
  {
    if ( enabled )
    {
      return ImGui::Button( label, size );
    }
    else
    {
      ImColor disabled_fg = ImColor( IM_COL32_BLACK );
      ImColor disabled_bg = ImColor( IM_COL32( 64, 64, 64, 255 ) );
      
      ImGui::PushStyleColor( ImGuiCol_Text, disabled_fg );
      ImGui::PushStyleColor( ImGuiCol_Button, disabled_bg );
      ImGui::PushStyleColor( ImGuiCol_ButtonHovered, disabled_bg );
      ImGui::PushStyleColor( ImGuiCol_ButtonActive, disabled_bg );
      ImGui::Button( label, size );
      ImGui::PopStyleColor( 4 );
      
      return false;
    }
  }
}
