// Mini memory editor for ImGui (to embed in your game/tools)
// v0.12
// Animated gif: https://cloud.githubusercontent.com/assets/8225057/9028162/3047ef88-392c-11e5-8270-a54f8354b208.gif
//
// You can adjust the keyboard repeat delay/rate in ImGuiIO.
// The code assume a mono-space font for simplicity! If you don't use the default font, use ImGui::PushFont()/PopFont() to switch to a mono-space font before caling this.
//
// Usage:
//   static MemoryEditor memory_editor;                                                     // save your state somewhere
//   memory_editor.Draw("Memory Editor", mem_block, mem_block_size, (size_t)mem_block);     // run
//
// Todo:
// - better resizing policy (ImGui doesn't have flexible window resizing constraints yet)
//
// Changelog:
// - v0.10: initial version
// - v0.11: always refreshing active text input with the latest byte from source memory if it's not being edited
// - v0.12: added RowsExtraSpacingCount to allow extra spacing every XX rows

struct MemoryEditor
{
    bool          Open;
    bool          AllowEdits;
    int           Rows;
    int           RowsExtraSpacingCount;        // Set to 0 to disable extra spacing between every XX rows
    int           DataEditingAddr;
    bool          DataEditingTakeFocus;
    char          DataInput[32];
    char          AddrInput[32];
    unsigned char (*ReadFn)(unsigned char* data, size_t off);
	void	      (*WriteFn)(unsigned char* data, size_t off, unsigned char d);

    MemoryEditor()
    {
        Open = true;
        Rows = 16;
        RowsExtraSpacingCount = 8;
        DataEditingAddr = -1;
        DataEditingTakeFocus = false;
        strcpy(DataInput, "");
        strcpy(AddrInput, "");
        AllowEdits = true;
        ReadFn = NULL;
        WriteFn = NULL;
    }

    void Draw(const char* title, unsigned char* mem_data, int mem_size, size_t base_display_addr = 0)
    {
        if (ImGui::Begin(title, &Open))
        {
            ImGui::BeginChild("##scrolling", ImVec2(0, -ImGui::GetItemsLineHeightWithSpacing()));

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));

            int addr_digits_count = 0;
            for (int n = base_display_addr + mem_size - 1; n > 0; n >>= 4)
                addr_digits_count++;

            float glyph_width = ImGui::CalcTextSize("F").x;
            float cell_width = glyph_width * 3; // "FF " we include trailing space in the width to easily catch clicks everywhere

            float line_height = ImGui::GetTextLineHeight();
            int line_total_count = (int)((mem_size + Rows-1) / Rows);
            ImGuiListClipper clipper(line_total_count, line_height);
            int visible_start_addr = clipper.DisplayStart * Rows;
            int visible_end_addr = clipper.DisplayEnd * Rows;

            bool data_next = false;

            if (!AllowEdits || DataEditingAddr >= mem_size)
                DataEditingAddr = -1;

            int data_editing_addr_backup = DataEditingAddr;
            if (DataEditingAddr != -1)
            {
                if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)) && DataEditingAddr >= Rows)                   { DataEditingAddr -= Rows; DataEditingTakeFocus = true; }
                else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)) && DataEditingAddr < mem_size - Rows)  { DataEditingAddr += Rows; DataEditingTakeFocus = true; }
                else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow)) && DataEditingAddr > 0)                { DataEditingAddr -= 1; DataEditingTakeFocus = true; }
                else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow)) && DataEditingAddr < mem_size - 1)    { DataEditingAddr += 1; DataEditingTakeFocus = true; }
            }
            if ((DataEditingAddr / Rows) != (data_editing_addr_backup / Rows))
            {
                // Track cursor movements
                float scroll_offset = ((DataEditingAddr / Rows) - (data_editing_addr_backup / Rows)) * line_height;
                bool scroll_desired = (scroll_offset < 0.0f && DataEditingAddr < visible_start_addr + Rows*2) || (scroll_offset > 0.0f && DataEditingAddr > visible_end_addr - Rows*2);
                if (scroll_desired)
                    ImGui::SetScrollY(ImGui::GetScrollY() + scroll_offset);
            }

            bool draw_separator_once = true;
            for (int line_i = clipper.DisplayStart; line_i < clipper.DisplayEnd; line_i++) // display only visible items
            {
                int addr = line_i * Rows;
                ImGui::Text("%0*X: ", addr_digits_count, base_display_addr+addr);
                ImGui::SameLine();

                // Draw Hexadecimal
                float line_start_x = ImGui::GetCursorPosX();
                for (int n = 0; n < Rows && addr < mem_size; n++, addr++)
                {
                    float byte_pos_x = line_start_x + cell_width * n;
                    if (RowsExtraSpacingCount > 1)
                        byte_pos_x += (n / RowsExtraSpacingCount) * cell_width * 0.25f; 
                    ImGui::SameLine(byte_pos_x);

                    if (DataEditingAddr == addr)
                    {
                        // Display text input on current byte
                        ImGui::PushID(addr);
                        bool data_write = false;
                        if (DataEditingTakeFocus)
                        {
                            ImGui::SetKeyboardFocusHere();
                            sprintf(AddrInput, "%0*X", addr_digits_count, base_display_addr+addr);
                            sprintf(DataInput, "%02X", ReadFn ? ReadFn(mem_data, addr) : mem_data[addr]);
                        }
                        ImGui::PushItemWidth(ImGui::CalcTextSize("FF").x);
                        struct UserData
                        {
                            // FIXME: We should have a way to retrieve the text edit cursor position more easily in the API, this is rather tedious.
                            static int Callback(ImGuiTextEditCallbackData* data)
                            {
                                UserData* user_data = (UserData*)data->UserData;
                                if (!data->HasSelection())
                                    user_data->CursorPos = data->CursorPos;
                                if (data->SelectionStart == 0 && data->SelectionEnd == data->BufTextLen)
                                {
                                    // When not editing a byte, always rewrite its content (this is a bit tricky, since InputText technically "owns" the master copy of the buffer we edit it in there)
                                    data->DeleteChars(0, data->BufTextLen);
                                    data->InsertChars(0, user_data->CurrentBufOverwrite);
                                    data->SelectionStart = 0;
                                    data->SelectionEnd = data->CursorPos = 2;
                                }
                                return 0;
                            }
                            char   CurrentBufOverwrite[3];  // Input
                            int    CursorPos;               // Output
                        };
                        UserData user_data;
                        sprintf(user_data.CurrentBufOverwrite, "%02X", ReadFn ? ReadFn(mem_data, addr) : mem_data[addr]);
                        ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsHexadecimal|ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_AutoSelectAll|ImGuiInputTextFlags_NoHorizontalScroll|ImGuiInputTextFlags_AlwaysInsertMode|ImGuiInputTextFlags_CallbackAlways;
                        if (ImGui::InputText("##data", DataInput, 32, flags, UserData::Callback, &user_data))
                            data_write = data_next = true;
                        else if (!DataEditingTakeFocus && !ImGui::IsItemActive())
                            DataEditingAddr = -1;
                        DataEditingTakeFocus = false;
                        ImGui::PopItemWidth();
                        if (user_data.CursorPos >= 2)
                            data_write = data_next = true;
                        if (data_write)
                        {
                            int data;
                            if (sscanf(DataInput, "%X", &data) == 1)
                                if (WriteFn)
                                    WriteFn(mem_data, addr, (unsigned char)data);
                                else
                                    mem_data[addr] = (unsigned char)data;
                        }
                        ImGui::PopID();
                    }
                    else
                    {
                        ImGui::Text("%02X ", ReadFn ? ReadFn(mem_data, addr) : mem_data[addr]);
                        if (AllowEdits && ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
                        {
                            DataEditingTakeFocus = true;
                            DataEditingAddr = addr;
                        }
                    }
                }

                float ascii_pos_x = line_start_x + cell_width * Rows + glyph_width * 1;
                if (RowsExtraSpacingCount > 0)
                    ascii_pos_x += ((Rows + RowsExtraSpacingCount - 1) / RowsExtraSpacingCount) * cell_width*0.25f;
                ImGui::SameLine(ascii_pos_x);

                if (draw_separator_once)
                {
                    ImVec2 screen_pos = ImGui::GetCursorScreenPos();
                    ImGui::GetWindowDrawList()->AddLine(ImVec2(screen_pos.x - glyph_width, screen_pos.y - 9999), ImVec2(screen_pos.x - glyph_width, screen_pos.y + 9999), ImGui::GetColorU32(ImGuiCol_Border));
                    draw_separator_once = false;
                }

                // Draw ASCII values
                addr = line_i * Rows;
                for (int n = 0; n < Rows && addr < mem_size; n++, addr++)
                {
                    if (n > 0) ImGui::SameLine();
                    int c = ReadFn ? ReadFn(mem_data, addr) : mem_data[addr];
                    ImGui::Text("%c", (c >= 32 && c < 128) ? c : '.');
                }
            }
            clipper.End();
            ImGui::PopStyleVar(2);

            ImGui::EndChild();

            if (data_next && DataEditingAddr < mem_size)
            {
                DataEditingAddr = DataEditingAddr + 1;
                DataEditingTakeFocus = true;
            }

            ImGui::Separator();

            ImGui::AlignFirstTextHeightToWidgets();
            ImGui::PushItemWidth(50);
            ImGui::PushAllowKeyboardFocus(false);
            int rows_backup = Rows;
            if (ImGui::DragInt("##rows", &Rows, 0.2f, 4, 32, "%.0f rows"))
            {
                ImVec2 new_window_size = ImGui::GetWindowSize();
                new_window_size.x += (Rows - rows_backup) * (cell_width + glyph_width);
                ImGui::SetWindowSize(new_window_size);
            }
            ImGui::PopAllowKeyboardFocus();
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::Text("Range %0*X..%0*X", addr_digits_count, (int)base_display_addr, addr_digits_count, (int)base_display_addr+mem_size-1);
            ImGui::SameLine();
            ImGui::PushItemWidth(70);
            if (ImGui::InputText("##addr", AddrInput, 32, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue))
            {
                int goto_addr;
                if (sscanf(AddrInput, "%X", &goto_addr) == 1)
                {
                    goto_addr -= base_display_addr;
                    if (goto_addr >= 0 && goto_addr < mem_size)
                    {
                        ImGui::BeginChild("##scrolling");
                        ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y + (goto_addr / Rows) * ImGui::GetTextLineHeight());
                        ImGui::EndChild();
                        DataEditingAddr = goto_addr;
                        DataEditingTakeFocus = true;
                    }
                }
            }
            ImGui::PopItemWidth();
        }
        ImGui::End();
    }
};
