// Mini memory editor for ImGui (to embed in your game/tools)
// Animated GIF: https://cloud.githubusercontent.com/assets/8225057/9028162/3047ef88-392c-11e5-8270-a54f8354b208.gif
//
// You can adjust the keyboard repeat delay/rate in ImGuiIO.
// The code assume a mono-space font for simplicity! If you don't use the default font, use ImGui::PushFont()/PopFont() to switch to a mono-space font before caling this.
//
// Usage:
//   static MemoryEditor mem_edit_1;                                        // store your state somewhere
//   mem_edit_1.Draw("Memory Editor", mem_block, mem_block_size, 0x0000);   // run
//
// Usage:
//   static MemoryEditor mem_edit_2;
//   mem_edit_2.Draw("Memory Editor", this, sizeof(*this), (size_t)this);
//
// Changelog:
// - v0.10: initial version
// - v0.11: always refresh active text input with the latest byte from source memory if it's not being edited.
// - v0.12: added RowsExtraSpacingCount to allow extra spacing every XX rows.
// - v0.13: added optional ReadFn/WriteFn handlers to access memory via a function. various warning fixes for 64-bits.
// - v0.14: added GotoAddr member, added GotoAddrAndHighlight() and highlighting. fixed minor scrollbar glitch when resizing.
// - v0.15: added maximum window width. minor optimization.
// - v0.16: added DrawZeroByteAsDisabledColor option. various sizing fixes when resizing using the "Rows" drag.
// - v0.17: added HighlightFn handler for optional non-contiguous highlighting.
// - v0.18: various fixes for displaying 64-bits addresses, fixed mouse click gaps introduced in recent changes.

struct MemoryEditor
{
    bool            Open;
    bool            ReadOnly;
    int             Rows;
    int             RowsExtraSpacingCount;        // Set to 0 to disable extra spacing between every XX rows
    size_t          DataEditingAddr;
    bool            DataEditingTakeFocus;
    char            DataInputBuf[32];
    char            AddrInputBuf[32];
    size_t          GotoAddr;
    size_t          HighlightMin, HighlightMax;
    ImU32           HighlightColor;
    bool            DrawZeroByteAsDisabledColor;
    unsigned char(*ReadFn)(unsigned char* data, size_t off);                     // optional function to read bytes
    void(*WriteFn)(unsigned char* data, size_t off, unsigned char d);   // optional function to write bytes
    bool(*HighlightFn)(unsigned char* data, size_t off);                // optional function to return Highlight property (to support non-contiguous highlighting)

    // Internals
    float           MaxWindowWidth;

    MemoryEditor()
    {
        Open = true;
        ReadOnly = false;
        Rows = 16;
        RowsExtraSpacingCount = 8;
        DataEditingAddr = (size_t)-1;
        DataEditingTakeFocus = false;
        strcpy(DataInputBuf, "");
        strcpy(AddrInputBuf, "");
        GotoAddr = (size_t)-1;
        HighlightMin = HighlightMax = (size_t)-1;
        HighlightColor = IM_COL32(255, 255, 255, 40);
        DrawZeroByteAsDisabledColor = true;
        ReadFn = NULL;
        WriteFn = NULL;
        HighlightFn = NULL;
        MaxWindowWidth = -1;
    }

    void GotoAddrAndHighlight(size_t addr_min, size_t addr_max)
    {
        GotoAddr = addr_min;
        HighlightMin = addr_min;
        HighlightMax = addr_max;
    }

    void Draw(const char* title, unsigned char* mem_data, size_t mem_size, size_t base_display_addr = 0x0000)
    {
        const float glyph_width = ImGui::CalcTextSize("F").x; // We assume the font is mono-space
        const float cell_width = glyph_width * 3;             // "FF " we include trailing space in the width to easily catch clicks everywhere
        const float extra_spacing = cell_width * 0.25f;       // Every RowsExtraSpacingCount columns we add a bit of extra spacing

        ImGuiStyle& style = ImGui::GetStyle();
        if (MaxWindowWidth > 0.0f)
            ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(MaxWindowWidth + style.ScrollbarSize + style.WindowPadding.x * 2 + glyph_width, FLT_MAX));

        /*Open = true;
        if (!ImGui::Begin(title, &Open, ImGuiWindowFlags_NoScrollbar))
        {
            ImGui::End();
            return;
        }*/

        ImGui::BeginChild("##scrolling", ImVec2(0, -ImGui::GetItemsLineHeightWithSpacing()));
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        int addr_digits_count = 0;
        for (size_t n = base_display_addr + mem_size - 1; n > 0; n >>= 4)
            addr_digits_count++;

        const float line_height = ImGui::GetTextLineHeight();
        const int line_total_count = (int)((mem_size + Rows - 1) / Rows);
        ImGuiListClipper clipper(line_total_count, line_height);
        const size_t visible_start_addr = clipper.DisplayStart * Rows;
        const size_t visible_end_addr = clipper.DisplayEnd * Rows;

        bool data_next = false;

        if (ReadOnly || DataEditingAddr >= mem_size)
            DataEditingAddr = (size_t)-1;

        size_t data_editing_addr_backup = DataEditingAddr;
        if (DataEditingAddr != (size_t)-1)
        {
            if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)) && DataEditingAddr >= (size_t)Rows) { DataEditingAddr -= Rows; DataEditingTakeFocus = true; }
            else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)) && DataEditingAddr < mem_size - Rows) { DataEditingAddr += Rows; DataEditingTakeFocus = true; }
            else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow)) && DataEditingAddr > 0) { DataEditingAddr -= 1; DataEditingTakeFocus = true; }
            else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow)) && DataEditingAddr < mem_size - 1) { DataEditingAddr += 1; DataEditingTakeFocus = true; }
        }
        if ((DataEditingAddr / Rows) != (data_editing_addr_backup / Rows))
        {
            // Track cursor movements
            const float scroll_offset = ((int)(DataEditingAddr / Rows) - (int)(data_editing_addr_backup / Rows)) * line_height;
            const bool scroll_desired = (scroll_offset < 0.0f && DataEditingAddr < visible_start_addr + Rows * 2) || (scroll_offset > 0.0f && DataEditingAddr > visible_end_addr - Rows * 2);
            if (scroll_desired)
                ImGui::SetScrollY(ImGui::GetScrollY() + scroll_offset);
        }

        bool draw_separator_once = true;
        for (int line_i = clipper.DisplayStart; line_i < clipper.DisplayEnd; line_i++) // display only visible lines
        {
            size_t addr = (size_t)(line_i * Rows);
            ImGui::Text("%0*llX: ", addr_digits_count, base_display_addr + addr);
            ImGui::SameLine();

            // Draw Hexadecimal
            float line_start_x = ImGui::GetCursorPosX();
            for (int n = 0; n < Rows && addr < mem_size; n++, addr++)
            {
                float byte_pos_x = line_start_x + cell_width * n;
                if (RowsExtraSpacingCount > 0)
                    byte_pos_x += (n / RowsExtraSpacingCount) * extra_spacing;
                ImGui::SameLine(byte_pos_x);

                // Draw highlight
                if ((addr >= HighlightMin && addr < HighlightMax) || (HighlightFn && HighlightFn(mem_data, addr)))
                {
                    ImVec2 pos = ImGui::GetCursorScreenPos();
                    float highlight_width = glyph_width * 2 + 2;
                    bool is_next_byte_highlighted = (n + 1 == Rows) || (addr + 1 < mem_size) && ((HighlightMax != (size_t)-1 && addr + 1 < HighlightMax) || (HighlightFn && HighlightFn(mem_data, addr + 1)));
                    if (is_next_byte_highlighted)
                    {
                        highlight_width = cell_width;
                        if (RowsExtraSpacingCount > 0 && n > 0 && (n + 1) < Rows && ((n + 1) % RowsExtraSpacingCount) == 0)
                            highlight_width += extra_spacing;
                    }
                    draw_list->AddRectFilled(pos, ImVec2(pos.x + highlight_width, pos.y + line_height), HighlightColor);
                }

                if (DataEditingAddr == addr)
                {
                    // Display text input on current byte
                    ImGui::PushID((void*)addr);
                    bool data_write = false;
                    if (DataEditingTakeFocus)
                    {
                        ImGui::SetKeyboardFocusHere();
                        sprintf(AddrInputBuf, "%0*llX", addr_digits_count, base_display_addr + addr);
                        sprintf(DataInputBuf, "%02X", ReadFn ? ReadFn(mem_data, addr) : mem_data[addr]);
                    }
                    ImGui::PushItemWidth(glyph_width * 2 + 1);
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
                    user_data.CursorPos = -1;
                    sprintf(user_data.CurrentBufOverwrite, "%02X", ReadFn ? ReadFn(mem_data, addr) : mem_data[addr]);
                    ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_NoHorizontalScroll | ImGuiInputTextFlags_AlwaysInsertMode | ImGuiInputTextFlags_CallbackAlways;
                    if (ImGui::InputText("##data", DataInputBuf, 32, flags, UserData::Callback, &user_data))
                        data_write = data_next = true;
                    else if (!DataEditingTakeFocus && !ImGui::IsItemActive())
                        DataEditingAddr = (size_t)-1;
                    DataEditingTakeFocus = false;
                    ImGui::PopItemWidth();
                    if (user_data.CursorPos >= 2)
                        data_write = data_next = true;
                    if (data_write)
                    {
                        int data;
                        if (sscanf(DataInputBuf, "%X", &data) == 1)
                        {
                            if (WriteFn)
                                WriteFn(mem_data, addr, (unsigned char)data);
                            else
                                mem_data[addr] = (unsigned char)data;
                        }
                    }
                    ImGui::PopID();
                }
                else
                {
                    // NB: The trailing space is not visible but ensure there's no gap that the mouse cannot click on.
                    unsigned char b = ReadFn ? ReadFn(mem_data, addr) : mem_data[addr];
                    if (b == 0 && DrawZeroByteAsDisabledColor)
                        ImGui::TextDisabled("00 ");
                    else
                        ImGui::Text("%02X ", b);
                    if (!ReadOnly && ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
                    {
                        DataEditingTakeFocus = true;
                        DataEditingAddr = addr;
                    }
                }
            }

            float ascii_pos_x = line_start_x + cell_width * Rows + glyph_width * 1;
            if (RowsExtraSpacingCount > 0)
                ascii_pos_x += ((Rows + RowsExtraSpacingCount - 1) / RowsExtraSpacingCount) * extra_spacing;

            ImGui::SameLine(ascii_pos_x);
            if (line_i == clipper.DisplayStart)
                MaxWindowWidth = ascii_pos_x + (Rows * glyph_width);

            // Vertical separator
            if (draw_separator_once)
            {
                ImVec2 screen_pos = ImGui::GetCursorScreenPos();
                draw_list->AddLine(ImVec2(screen_pos.x - glyph_width, screen_pos.y - 9999), ImVec2(screen_pos.x - glyph_width, screen_pos.y + 9999), ImGui::GetColorU32(ImGuiCol_Border));
                draw_separator_once = false;
            }

            // Draw ASCII values
            ImVec2 pos = ImGui::GetCursorScreenPos();
            addr = line_i * Rows;
            const ImU32 color_text = ImGui::GetColorU32(ImGuiCol_Text);
            for (int n = 0; n < Rows && addr < mem_size; n++, addr++)
            {
                int c = ReadFn ? ReadFn(mem_data, addr) : mem_data[addr];
                char c_display = (c >= 32 && c < 128) ? (char)c : '.';
                draw_list->AddText(pos, color_text, &c_display, &c_display + 1);
                pos.x += glyph_width;
            }
            ImGui::Dummy(ImVec2(0, 0));
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
        ImGui::PushItemWidth(56);
        ImGui::PushAllowKeyboardFocus(false);
        int rows_backup = Rows;
        if (ImGui::DragInt("##rows", &Rows, 0.2f, 4, 32, "%.0f rows"))
        {
            float size_dx = (Rows - rows_backup) * (cell_width + glyph_width);
            if (RowsExtraSpacingCount > 0)
                size_dx += (((Rows + RowsExtraSpacingCount - 1) / RowsExtraSpacingCount) - ((rows_backup + RowsExtraSpacingCount - 1) / RowsExtraSpacingCount)) * extra_spacing;
            ImVec2 new_window_size = ImGui::GetWindowSize();
            new_window_size.x += size_dx;
            if (MaxWindowWidth > 0)
                MaxWindowWidth += size_dx;
            ImGui::SetWindowSize(new_window_size);
        }
        ImGui::PopAllowKeyboardFocus();
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::Text("Range %0*llX..%0*llX", addr_digits_count, base_display_addr, addr_digits_count, base_display_addr + mem_size - 1);
        ImGui::SameLine();
        ImGui::PushItemWidth(addr_digits_count * (glyph_width + 1) + style.FramePadding.x * 2.0f);
        if (ImGui::InputText("##addr", AddrInputBuf, 32, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue))
        {
            size_t goto_addr;
            if (sscanf(AddrInputBuf, "%llX", &goto_addr) == 1)
            {
                GotoAddr = goto_addr - base_display_addr;
                HighlightMin = HighlightMax = (size_t)-1;
            }
        }
        ImGui::PopItemWidth();

        if (GotoAddr != (size_t)-1)
        {
            if (GotoAddr >= 0 && GotoAddr < mem_size)
            {
                ImGui::BeginChild("##scrolling");
                ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y + (GotoAddr / Rows) * ImGui::GetTextLineHeight());
                ImGui::EndChild();
                DataEditingAddr = GotoAddr;
                DataEditingTakeFocus = true;
            }
            GotoAddr = (size_t)-1;
        }

        //ImGui::End();
    }
};
