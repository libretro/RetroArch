//- Common Code For All Addons needed just to ease inclusion as separate files in user code ----------------------
#include <imgui.h>
#undef IMGUI_DEFINE_PLACEMENT_NEW
#define IMGUI_DEFINE_PLACEMENT_NEW
#undef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
//-----------------------------------------------------------------------------------------------------------------

#include "imguihelper.h"

#if 0
#ifdef _WIN32
#include <shellapi.h>	// ShellExecuteA(...) - Shell32.lib
#include <objbase.h>    // CoInitializeEx(...)  - ole32.lib
#else //_WIN32
#include <unistd.h>
#include <stdlib.h> // system
#endif //_WIN32
#endif

#include <imgui_internal.h>

#ifndef NO_IMGUIHELPER_DRAW_METHODS
#  if (!defined(alloca))
#      ifdef _WIN32
#          include <malloc.h>     // alloca
#      elif (defined(__FreeBSD__) || defined(FreeBSD_kernel) || defined(__DragonFly__)) && !defined(__GLIBC__)
#          include <stdlib.h>     // alloca. FreeBSD uses stdlib.h unless GLIBC
#      else //_WIN32
#           include <alloca.h>     // alloca
#       endif //_WIN32
#  endif // alloca
#endif //NO_IMGUIHELPER_DRAW_METHODS


namespace ImGui {

#if 0
bool OpenWithDefaultApplication(const char* url,bool exploreModeForWindowsOS)	{
#       ifdef _WIN32
            //CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);  // Needed ??? Well, let's suppose the user initializes it himself for now"
            return ((size_t)ShellExecuteA( NULL, exploreModeForWindowsOS ? "explore" : "open", url, "", ".", SW_SHOWNORMAL ))>32;
#       else //_WIN32
            if (exploreModeForWindowsOS) exploreModeForWindowsOS = false;   // No warnings
            char tmp[4096];
            const char* openPrograms[]={"xdg-open","gnome-open"};	// Not sure what can I append here for MacOS

            static int openProgramIndex=-2;
            if (openProgramIndex==-2)   {
                openProgramIndex=-1;
                for (size_t i=0,sz=sizeof(openPrograms)/sizeof(openPrograms[0]);i<sz;i++) {
                    strcpy(tmp,"/usr/bin/");	// Well, we should check all the folders inside $PATH... and we ASSUME that /usr/bin IS inside $PATH (see below)
                    strcat(tmp,openPrograms[i]);
                    FILE* fd = ImFileOpen(tmp,"r");
                    if (fd) {
                        fclose(fd);
                        openProgramIndex = (int)i;
                        //printf(stderr,"found %s\n",tmp);
                        break;
                    }
                }
            }

            // Note that here we strip the prefix "/usr/bin" and just use openPrograms[openProgramsIndex].
            // Also note that if nothing has been found we use "xdg-open" (that might still work if it exists in $PATH, but not in /usr/bin).
            strcpy(tmp,openPrograms[openProgramIndex<0?0:openProgramIndex]);

            strcat(tmp," \"");
            strcat(tmp,url);
            strcat(tmp,"\"");
            return system(tmp)==0;
#       endif //_WIN32
}
#endif

void CloseAllPopupMenus()   {
    ImGuiContext& g = *GImGui;
    while (g.OpenPopupStack.size() > 0) g.OpenPopupStack.pop_back();
}

// Posted by Omar in one post. It might turn useful...
bool IsItemActiveLastFrame()    {
    ImGuiContext& g = *GImGui;
    if (g.ActiveIdPreviousFrame)
        return g.ActiveIdPreviousFrame== GImGui->CurrentWindow->DC.LastItemId;
    return false;
}
bool IsItemJustReleased()   {
    return IsItemActiveLastFrame() && !ImGui::IsItemActive();
}


#ifndef NO_IMGUIHELPER_FONT_METHODS
const ImFont *GetFont(int fntIndex) {return (fntIndex>=0 && fntIndex<ImGui::GetIO().Fonts->Fonts.size()) ? ImGui::GetIO().Fonts->Fonts[fntIndex] : NULL;}
void PushFont(int fntIndex)    {
    IM_ASSERT(fntIndex>=0 && fntIndex<ImGui::GetIO().Fonts->Fonts.size());
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[fntIndex]);
}
void TextColoredV(int fntIndex, const ImVec4 &col, const char *fmt, va_list args) {
    ImGui::PushFont(fntIndex);
    ImGui::TextColoredV(col,fmt, args);
    ImGui::PopFont();
}
void TextColored(int fntIndex, const ImVec4 &col, const char *fmt,...)  {
    va_list args;
    va_start(args, fmt);
    TextColoredV(fntIndex,col, fmt, args);
    va_end(args);
}
void TextV(int fntIndex, const char *fmt, va_list args) {
    if (ImGui::GetCurrentWindow()->SkipItems) return;

    ImGuiContext& g = *GImGui;
    const char* text_end = g.TempBuffer + ImFormatStringV(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), fmt, args);
    ImGui::PushFont(fntIndex);
    TextUnformatted(g.TempBuffer, text_end);
    ImGui::PopFont();
}
void Text(int fntIndex, const char *fmt,...)    {
    va_list args;
    va_start(args, fmt);
    TextV(fntIndex,fmt, args);
    va_end(args);
}

bool GetTexCoordsFromGlyph(unsigned short glyph, ImVec2 &uv0, ImVec2 &uv1) {
    if (!GImGui->Font) return false;
    const ImFont::Glyph* g = GImGui->Font->FindGlyph(glyph);
    if (g)  {
        uv0.x = g->U0; uv0.y = g->V0;
        uv1.x = g->U1; uv1.y = g->V1;
        return true;
    }
    return false;
}
float CalcMainMenuHeight()  {
    if (GImGui->FontBaseSize>0) return GImGui->FontBaseSize + GImGui->Style.FramePadding.y * 2.0f;
    else {
        ImGuiIO& io = ImGui::GetIO();
        ImGuiStyle& style = ImGui::GetStyle();
        ImFont* font = ImGui::GetFont();
        if (!font) {
            if (io.Fonts->Fonts.size()>0) font = io.Fonts->Fonts[0];
            else return (14)+style.FramePadding.y * 2.0f;
        }
        return (io.FontGlobalScale * font->Scale * font->FontSize) + style.FramePadding.y * 2.0f;
    }
}
#endif //NO_IMGUIHELPER_FONT_METHODS

#ifndef NO_IMGUIHELPER_DRAW_METHODS
inline static void GetVerticalGradientTopAndBottomColors(ImU32 c,float fillColorGradientDeltaIn0_05,ImU32& tc,ImU32& bc)  {
    if (fillColorGradientDeltaIn0_05==0) {tc=bc=c;return;}

    static ImU32 cacheColorIn=0;static float cacheGradientIn=0.f;static ImU32 cacheTopColorOut=0;static ImU32 cacheBottomColorOut=0;
    if (cacheColorIn==c && cacheGradientIn==fillColorGradientDeltaIn0_05)   {tc=cacheTopColorOut;bc=cacheBottomColorOut;return;}
    cacheColorIn=c;cacheGradientIn=fillColorGradientDeltaIn0_05;

    const bool negative = (fillColorGradientDeltaIn0_05<0);
    if (negative) fillColorGradientDeltaIn0_05=-fillColorGradientDeltaIn0_05;
    if (fillColorGradientDeltaIn0_05>0.5f) fillColorGradientDeltaIn0_05=0.5f;


    // New code:
    //#define IM_COL32(R,G,B,A)    (((ImU32)(A)<<IM_COL32_A_SHIFT) | ((ImU32)(B)<<IM_COL32_B_SHIFT) | ((ImU32)(G)<<IM_COL32_G_SHIFT) | ((ImU32)(R)<<IM_COL32_R_SHIFT))
    const int fcgi = fillColorGradientDeltaIn0_05*255.0f;
    const int R = (unsigned char) (c>>IM_COL32_R_SHIFT);    // The cast should reset upper bits (as far as I hope)
    const int G = (unsigned char) (c>>IM_COL32_G_SHIFT);
    const int B = (unsigned char) (c>>IM_COL32_B_SHIFT);
    const int A = (unsigned char) (c>>IM_COL32_A_SHIFT);

    int r = R+fcgi, g = G+fcgi, b = B+fcgi;
    if (r>255) r=255;if (g>255) g=255;if (b>255) b=255;
    if (negative) bc = IM_COL32(r,g,b,A); else tc = IM_COL32(r,g,b,A);

    r = R-fcgi; g = G-fcgi; b = B-fcgi;
    if (r<0) r=0;if (g<0) g=0;if (b<0) b=0;
    if (negative) tc = IM_COL32(r,g,b,A); else bc = IM_COL32(r,g,b,A);

    // Old legacy code (to remove)... [However here we lerp alpha too...]
    /*// Can we do it without the double conversion ImU32 -> ImVec4 -> ImU32 ?
    const ImVec4 cf = ColorConvertU32ToFloat4(c);
    ImVec4 tmp(cf.x+fillColorGradientDeltaIn0_05,cf.y+fillColorGradientDeltaIn0_05,cf.z+fillColorGradientDeltaIn0_05,cf.w+fillColorGradientDeltaIn0_05);
    if (tmp.x>1.f) tmp.x=1.f;if (tmp.y>1.f) tmp.y=1.f;if (tmp.z>1.f) tmp.z=1.f;if (tmp.w>1.f) tmp.w=1.f;
    if (negative) bc = ColorConvertFloat4ToU32(tmp); else tc = ColorConvertFloat4ToU32(tmp);
    tmp=ImVec4(cf.x-fillColorGradientDeltaIn0_05,cf.y-fillColorGradientDeltaIn0_05,cf.z-fillColorGradientDeltaIn0_05,cf.w-fillColorGradientDeltaIn0_05);
    if (tmp.x<0.f) tmp.x=0.f;if (tmp.y<0.f) tmp.y=0.f;if (tmp.z<0.f) tmp.z=0.f;if (tmp.w<0.f) tmp.w=0.f;
    if (negative) tc = ColorConvertFloat4ToU32(tmp); else bc = ColorConvertFloat4ToU32(tmp);*/


    cacheTopColorOut=tc;cacheBottomColorOut=bc;
}
inline static ImU32 GetVerticalGradient(const ImVec4& ct,const ImVec4& cb,float DH,float H)    {
    IM_ASSERT(H!=0);
    const float fa = DH/H;
    const float fc = (1.f-fa);
    return ColorConvertFloat4ToU32(ImVec4(
        ct.x * fc + cb.x * fa,
        ct.y * fc + cb.y * fa,
        ct.z * fc + cb.z * fa,
        ct.w * fc + cb.w * fa)
    );
}
void ImDrawListAddConvexPolyFilledWithVerticalGradient(ImDrawList *dl, const ImVec2 *points, const int points_count, ImU32 colTop, ImU32 colBot, bool anti_aliased,float miny,float maxy)
{
    if (!dl) return;
    if (colTop==colBot)  {
        dl->AddConvexPolyFilled(points,points_count,colTop,anti_aliased);
        return;
    }
    const ImVec2 uv = GImGui->FontTexUvWhitePixel;
    anti_aliased &= GImGui->Style.AntiAliasedShapes;
    //if (ImGui::GetIO().KeyCtrl) anti_aliased = false; // Debug

    int height=0;
    if (miny<=0 || maxy<=0) {
        const float max_float = 999999999999999999.f;
        miny=max_float;maxy=-max_float;
        for (int i = 0; i < points_count; i++) {
            const float h = points[i].y;
            if (h < miny) miny = h;
            else if (h > maxy) maxy = h;
        }
    }
    height = maxy-miny;
    const ImVec4 colTopf = ColorConvertU32ToFloat4(colTop);
    const ImVec4 colBotf = ColorConvertU32ToFloat4(colBot);


    if (anti_aliased)
    {
        // Anti-aliased Fill
        const float AA_SIZE = 1.0f;

        const ImVec4 colTransTopf(colTopf.x,colTopf.y,colTopf.z,0.f);
        const ImVec4 colTransBotf(colBotf.x,colBotf.y,colBotf.z,0.f);
        const int idx_count = (points_count-2)*3 + points_count*6;
        const int vtx_count = (points_count*2);
        dl->PrimReserve(idx_count, vtx_count);

        // Add indexes for fill
        unsigned int vtx_inner_idx = dl->_VtxCurrentIdx;
        unsigned int vtx_outer_idx = dl->_VtxCurrentIdx+1;
        for (int i = 2; i < points_count; i++)
        {
            dl->_IdxWritePtr[0] = (ImDrawIdx)(vtx_inner_idx); dl->_IdxWritePtr[1] = (ImDrawIdx)(vtx_inner_idx+((i-1)<<1)); dl->_IdxWritePtr[2] = (ImDrawIdx)(vtx_inner_idx+(i<<1));
            dl->_IdxWritePtr += 3;
        }

        // Compute normals
        ImVec2* temp_normals = (ImVec2*)alloca(points_count * sizeof(ImVec2));
        for (int i0 = points_count-1, i1 = 0; i1 < points_count; i0 = i1++)
        {
            const ImVec2& p0 = points[i0];
            const ImVec2& p1 = points[i1];
            ImVec2 diff = p1 - p0;
            diff *= ImInvLength(diff, 1.0f);
            temp_normals[i0].x = diff.y;
            temp_normals[i0].y = -diff.x;
        }

        for (int i0 = points_count-1, i1 = 0; i1 < points_count; i0 = i1++)
        {
            // Average normals
            const ImVec2& n0 = temp_normals[i0];
            const ImVec2& n1 = temp_normals[i1];
            ImVec2 dm = (n0 + n1) * 0.5f;
            float dmr2 = dm.x*dm.x + dm.y*dm.y;
            if (dmr2 > 0.000001f)
            {
                float scale = 1.0f / dmr2;
                if (scale > 100.0f) scale = 100.0f;
                dm *= scale;
            }
            dm *= AA_SIZE * 0.5f;

            // Add vertices
            //_VtxWritePtr[0].pos = (points[i1] - dm); _VtxWritePtr[0].uv = uv; _VtxWritePtr[0].col = col;        // Inner
            //_VtxWritePtr[1].pos = (points[i1] + dm); _VtxWritePtr[1].uv = uv; _VtxWritePtr[1].col = col_trans;  // Outer
            dl->_VtxWritePtr[0].pos = (points[i1] - dm); dl->_VtxWritePtr[0].uv = uv; dl->_VtxWritePtr[0].col = GetVerticalGradient(colTopf,colBotf,points[i1].y-miny,height);        // Inner
            dl->_VtxWritePtr[1].pos = (points[i1] + dm); dl->_VtxWritePtr[1].uv = uv; dl->_VtxWritePtr[1].col = GetVerticalGradient(colTransTopf,colTransBotf,points[i1].y-miny,height);  // Outer
            dl->_VtxWritePtr += 2;

            // Add indexes for fringes
            dl->_IdxWritePtr[0] = (ImDrawIdx)(vtx_inner_idx+(i1<<1)); dl->_IdxWritePtr[1] = (ImDrawIdx)(vtx_inner_idx+(i0<<1)); dl->_IdxWritePtr[2] = (ImDrawIdx)(vtx_outer_idx+(i0<<1));
            dl->_IdxWritePtr[3] = (ImDrawIdx)(vtx_outer_idx+(i0<<1)); dl->_IdxWritePtr[4] = (ImDrawIdx)(vtx_outer_idx+(i1<<1)); dl->_IdxWritePtr[5] = (ImDrawIdx)(vtx_inner_idx+(i1<<1));
            dl->_IdxWritePtr += 6;
        }
        dl->_VtxCurrentIdx += (ImDrawIdx)vtx_count;
    }
    else
    {
        // Non Anti-aliased Fill
        const int idx_count = (points_count-2)*3;
        const int vtx_count = points_count;
        dl->PrimReserve(idx_count, vtx_count);
        for (int i = 0; i < vtx_count; i++)
        {
            //_VtxWritePtr[0].pos = points[i]; _VtxWritePtr[0].uv = uv; _VtxWritePtr[0].col = col;
            dl->_VtxWritePtr[0].pos = points[i]; dl->_VtxWritePtr[0].uv = uv; dl->_VtxWritePtr[0].col = GetVerticalGradient(colTopf,colBotf,points[i].y-miny,height);
            dl->_VtxWritePtr++;
        }
        for (int i = 2; i < points_count; i++)
        {
            dl->_IdxWritePtr[0] = (ImDrawIdx)(dl->_VtxCurrentIdx); dl->_IdxWritePtr[1] = (ImDrawIdx)(dl->_VtxCurrentIdx+i-1); dl->_IdxWritePtr[2] = (ImDrawIdx)(dl->_VtxCurrentIdx+i);
            dl->_IdxWritePtr += 3;
        }
        dl->_VtxCurrentIdx += (ImDrawIdx)vtx_count;
    }
}
void ImDrawListPathFillWithVerticalGradientAndStroke(ImDrawList *dl, const ImU32 &fillColorTop, const ImU32 &fillColorBottom, const ImU32 &strokeColor, bool strokeClosed, float strokeThickness, bool antiAliased,float miny,float maxy)    {
    if (!dl) return;
    if (fillColorTop==fillColorBottom) dl->AddConvexPolyFilled(dl->_Path.Data,dl->_Path.Size, fillColorTop, antiAliased);
    else if ((fillColorTop & IM_COL32_A_MASK) != 0 || (fillColorBottom & IM_COL32_A_MASK) != 0) ImDrawListAddConvexPolyFilledWithVerticalGradient(dl, dl->_Path.Data, dl->_Path.Size, fillColorTop, fillColorBottom, antiAliased,miny,maxy);
    if ((strokeColor& IM_COL32_A_MASK)!= 0 && strokeThickness>0) dl->AddPolyline(dl->_Path.Data, dl->_Path.Size, strokeColor, strokeClosed, strokeThickness, antiAliased);
    dl->PathClear();
}
void ImDrawListPathFillAndStroke(ImDrawList *dl, const ImU32 &fillColor, const ImU32 &strokeColor, bool strokeClosed, float strokeThickness, bool antiAliased)    {
    if (!dl) return;
    if ((fillColor & IM_COL32_A_MASK) != 0) dl->AddConvexPolyFilled(dl->_Path.Data, dl->_Path.Size, fillColor, antiAliased);
    if ((strokeColor& IM_COL32_A_MASK)!= 0 && strokeThickness>0) dl->AddPolyline(dl->_Path.Data, dl->_Path.Size, strokeColor, strokeClosed, strokeThickness, antiAliased);
    dl->PathClear();
}
void ImDrawListAddRect(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, const ImU32 &fillColor, const ImU32 &strokeColor, float rounding, int rounding_corners, float strokeThickness, bool antiAliased) {
    if (!dl || (((fillColor & IM_COL32_A_MASK) == 0) && ((strokeColor & IM_COL32_A_MASK) == 0)))  return;
    dl->PathRect(a, b, rounding, rounding_corners);
    ImDrawListPathFillAndStroke(dl,fillColor,strokeColor,true,strokeThickness,antiAliased);
}
void ImDrawListAddRectWithVerticalGradient(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, const ImU32 &fillColorTop, const ImU32 &fillColorBottom, const ImU32 &strokeColor, float rounding, int rounding_corners, float strokeThickness, bool antiAliased) {
    if (!dl || (((fillColorTop & IM_COL32_A_MASK) == 0) && ((fillColorBottom & IM_COL32_A_MASK) == 0) && ((strokeColor & IM_COL32_A_MASK) == 0)))  return;
    if (rounding==0.f || rounding_corners==0) {
        dl->AddRectFilledMultiColor(a,b,fillColorTop,fillColorTop,fillColorBottom,fillColorBottom); // Huge speedup!
        if ((strokeColor& IM_COL32_A_MASK)!= 0 && strokeThickness>0.f) {
            dl->PathRect(a, b, rounding, rounding_corners);
            dl->AddPolyline(dl->_Path.Data, dl->_Path.Size, strokeColor, true, strokeThickness, antiAliased);
            dl->PathClear();
        }
    }
    else    {
        dl->PathRect(a, b, rounding, rounding_corners);
        ImDrawListPathFillWithVerticalGradientAndStroke(dl,fillColorTop,fillColorBottom,strokeColor,true,strokeThickness,antiAliased,a.y,b.y);
    }
}
void ImDrawListPathArcTo(ImDrawList *dl, const ImVec2 &centre, const ImVec2 &radii, float amin, float amax, int num_segments)  {
    if (!dl) return;
    if (radii.x == 0.0f || radii.y==0) dl->_Path.push_back(centre);
    dl->_Path.reserve(dl->_Path.Size + (num_segments + 1));
    for (int i = 0; i <= num_segments; i++)
    {
        const float a = amin + ((float)i / (float)num_segments) * (amax - amin);
        dl->_Path.push_back(ImVec2(centre.x + cosf(a) * radii.x, centre.y + sinf(a) * radii.y));
    }
}
void ImDrawListAddEllipse(ImDrawList *dl, const ImVec2 &centre, const ImVec2 &radii, const ImU32 &fillColor, const ImU32 &strokeColor, int num_segments, float strokeThickness, bool antiAliased)   {
    if (!dl) return;
    const float a_max = IM_PI*2.0f * ((float)num_segments - 1.0f) / (float)num_segments;
    ImDrawListPathArcTo(dl,centre, radii, 0.0f, a_max, num_segments);
    ImDrawListPathFillAndStroke(dl,fillColor,strokeColor,true,strokeThickness,antiAliased);
}
void ImDrawListAddEllipseWithVerticalGradient(ImDrawList *dl, const ImVec2 &centre, const ImVec2 &radii, const ImU32 &fillColorTop, const ImU32 &fillColorBottom, const ImU32 &strokeColor, int num_segments, float strokeThickness, bool antiAliased)   {
    if (!dl) return;
    const float a_max = IM_PI*2.0f * ((float)num_segments - 1.0f) / (float)num_segments;
    ImDrawListPathArcTo(dl,centre, radii, 0.0f, a_max, num_segments);
    ImDrawListPathFillWithVerticalGradientAndStroke(dl,fillColorTop,fillColorBottom,strokeColor,true,strokeThickness,antiAliased,centre.y-radii.y,centre.y+radii.y);
}
void ImDrawListAddCircle(ImDrawList *dl, const ImVec2 &centre, float radius, const ImU32 &fillColor, const ImU32 &strokeColor, int num_segments, float strokeThickness, bool antiAliased)   {
    if (!dl) return;
    const ImVec2 radii(radius,radius);
    const float a_max = IM_PI*2.0f * ((float)num_segments - 1.0f) / (float)num_segments;
    ImDrawListPathArcTo(dl,centre, radii, 0.0f, a_max, num_segments);
    ImDrawListPathFillAndStroke(dl,fillColor,strokeColor,true,strokeThickness,antiAliased);
}
void ImDrawListAddCircleWithVerticalGradient(ImDrawList *dl, const ImVec2 &centre, float radius, const ImU32 &fillColorTop, const ImU32 &fillColorBottom, const ImU32 &strokeColor, int num_segments, float strokeThickness, bool antiAliased)   {
    if (!dl) return;
    const ImVec2 radii(radius,radius);
    const float a_max = IM_PI*2.0f * ((float)num_segments - 1.0f) / (float)num_segments;
    ImDrawListPathArcTo(dl,centre, radii, 0.0f, a_max, num_segments);
    ImDrawListPathFillWithVerticalGradientAndStroke(dl,fillColorTop,fillColorBottom,strokeColor,true,strokeThickness,antiAliased,centre.y-radius,centre.y+radius);
}
void ImDrawListAddRectWithVerticalGradient(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, const ImU32 &fillColor, float fillColorGradientDeltaIn0_05, const ImU32 &strokeColor, float rounding, int rounding_corners, float strokeThickness, bool antiAliased)   {
    ImU32 fillColorTop,fillColorBottom;GetVerticalGradientTopAndBottomColors(fillColor,fillColorGradientDeltaIn0_05,fillColorTop,fillColorBottom);
    ImDrawListAddRectWithVerticalGradient(dl,a,b,fillColorTop,fillColorBottom,strokeColor,rounding,rounding_corners,strokeThickness,antiAliased);
}
void ImDrawListAddPolyLine(ImDrawList *dl, const ImVec2* polyPoints,int numPolyPoints, ImU32 strokeColor,float strokeThickness,bool strokeClosed, const ImVec2 &offset, const ImVec2 &scale,bool antiAliased) {
    if (polyPoints && numPolyPoints>0 && (strokeColor & IM_COL32_A_MASK) != 0) {
	static ImVector<ImVec2> points;
	points.resize(numPolyPoints);
	for (int i=0;i<numPolyPoints;i++)   points[i] = offset + polyPoints[i]*scale;
	dl->AddPolyline(&points[0],points.size(),strokeColor,strokeClosed,strokeThickness,antiAliased);
    }
}


// Should I put these methods inside  NO_IMGUIHELPER_VERTICAL_TEXT_METHODS ?
void ImDrawListAddConvexPolyFilledWithHorizontalGradient(ImDrawList *dl, const ImVec2 *points, const int points_count, ImU32 colLeft, ImU32 colRight, bool anti_aliased, float minx, float maxx)
{
    if (!dl) return;
    if (colLeft==colRight)  {
        dl->AddConvexPolyFilled(points,points_count,colLeft,anti_aliased);
        return;
    }
    const ImVec2 uv = GImGui->FontTexUvWhitePixel;
    anti_aliased &= GImGui->Style.AntiAliasedShapes;
    //if (ImGui::GetIO().KeyCtrl) anti_aliased = false; // Debug

    int width=0;
    if (minx<=0 || maxx<=0) {
        const float max_float = 999999999999999999.f;
        minx=max_float;maxx=-max_float;
        for (int i = 0; i < points_count; i++) {
            const float w = points[i].x;
            if (w < minx) minx = w;
            else if (w > maxx) maxx = w;
        }
    }
    width = maxx-minx;
    const ImVec4 colLeftf  = ColorConvertU32ToFloat4(colLeft);
    const ImVec4 colRightf = ColorConvertU32ToFloat4(colRight);


    if (anti_aliased)
    {
        // Anti-aliased Fill
        const float AA_SIZE = 1.0f;

        const ImVec4 colTransLeftf(colLeftf.x,colLeftf.y,colLeftf.z,0.f);
        const ImVec4 colTransRightf(colRightf.x,colRightf.y,colRightf.z,0.f);
        const int idx_count = (points_count-2)*3 + points_count*6;
        const int vtx_count = (points_count*2);
        dl->PrimReserve(idx_count, vtx_count);

        // Add indexes for fill
        unsigned int vtx_inner_idx = dl->_VtxCurrentIdx;
        unsigned int vtx_outer_idx = dl->_VtxCurrentIdx+1;
        for (int i = 2; i < points_count; i++)
        {
            dl->_IdxWritePtr[0] = (ImDrawIdx)(vtx_inner_idx); dl->_IdxWritePtr[1] = (ImDrawIdx)(vtx_inner_idx+((i-1)<<1)); dl->_IdxWritePtr[2] = (ImDrawIdx)(vtx_inner_idx+(i<<1));
            dl->_IdxWritePtr += 3;
        }

        // Compute normals
        ImVec2* temp_normals = (ImVec2*)alloca(points_count * sizeof(ImVec2));
        for (int i0 = points_count-1, i1 = 0; i1 < points_count; i0 = i1++)
        {
            const ImVec2& p0 = points[i0];
            const ImVec2& p1 = points[i1];
            ImVec2 diff = p1 - p0;
            diff *= ImInvLength(diff, 1.0f);
            temp_normals[i0].x = diff.y;
            temp_normals[i0].y = -diff.x;
        }

        for (int i0 = points_count-1, i1 = 0; i1 < points_count; i0 = i1++)
        {
            // Average normals
            const ImVec2& n0 = temp_normals[i0];
            const ImVec2& n1 = temp_normals[i1];
            ImVec2 dm = (n0 + n1) * 0.5f;
            float dmr2 = dm.x*dm.x + dm.y*dm.y;
            if (dmr2 > 0.000001f)
            {
                float scale = 1.0f / dmr2;
                if (scale > 100.0f) scale = 100.0f;
                dm *= scale;
            }
            dm *= AA_SIZE * 0.5f;

            // Add vertices
            //_VtxWritePtr[0].pos = (points[i1] - dm); _VtxWritePtr[0].uv = uv; _VtxWritePtr[0].col = col;        // Inner
            //_VtxWritePtr[1].pos = (points[i1] + dm); _VtxWritePtr[1].uv = uv; _VtxWritePtr[1].col = col_trans;  // Outer
            dl->_VtxWritePtr[0].pos = (points[i1] - dm); dl->_VtxWritePtr[0].uv = uv; dl->_VtxWritePtr[0].col = GetVerticalGradient(colLeftf,colRightf,points[i1].x-minx,width);        // Inner
            dl->_VtxWritePtr[1].pos = (points[i1] + dm); dl->_VtxWritePtr[1].uv = uv; dl->_VtxWritePtr[1].col = GetVerticalGradient(colTransLeftf,colTransRightf,points[i1].x-minx,width);  // Outer
            dl->_VtxWritePtr += 2;

            // Add indexes for fringes
            dl->_IdxWritePtr[0] = (ImDrawIdx)(vtx_inner_idx+(i1<<1)); dl->_IdxWritePtr[1] = (ImDrawIdx)(vtx_inner_idx+(i0<<1)); dl->_IdxWritePtr[2] = (ImDrawIdx)(vtx_outer_idx+(i0<<1));
            dl->_IdxWritePtr[3] = (ImDrawIdx)(vtx_outer_idx+(i0<<1)); dl->_IdxWritePtr[4] = (ImDrawIdx)(vtx_outer_idx+(i1<<1)); dl->_IdxWritePtr[5] = (ImDrawIdx)(vtx_inner_idx+(i1<<1));
            dl->_IdxWritePtr += 6;
        }
        dl->_VtxCurrentIdx += (ImDrawIdx)vtx_count;
    }
    else
    {
        // Non Anti-aliased Fill
        const int idx_count = (points_count-2)*3;
        const int vtx_count = points_count;
        dl->PrimReserve(idx_count, vtx_count);
        for (int i = 0; i < vtx_count; i++)
        {
            //_VtxWritePtr[0].pos = points[i]; _VtxWritePtr[0].uv = uv; _VtxWritePtr[0].col = col;
            dl->_VtxWritePtr[0].pos = points[i]; dl->_VtxWritePtr[0].uv = uv; dl->_VtxWritePtr[0].col = GetVerticalGradient(colLeftf,colRightf,points[i].x-minx,width);
            dl->_VtxWritePtr++;
        }
        for (int i = 2; i < points_count; i++)
        {
            dl->_IdxWritePtr[0] = (ImDrawIdx)(dl->_VtxCurrentIdx); dl->_IdxWritePtr[1] = (ImDrawIdx)(dl->_VtxCurrentIdx+i-1); dl->_IdxWritePtr[2] = (ImDrawIdx)(dl->_VtxCurrentIdx+i);
            dl->_IdxWritePtr += 3;
        }
        dl->_VtxCurrentIdx += (ImDrawIdx)vtx_count;
    }
}
void ImDrawListPathFillWithHorizontalGradientAndStroke(ImDrawList *dl, const ImU32 &fillColorLeft, const ImU32 &fillColorRight, const ImU32 &strokeColor, bool strokeClosed, float strokeThickness, bool antiAliased, float minx, float maxx)    {
    if (!dl) return;
    if (fillColorLeft==fillColorRight) dl->AddConvexPolyFilled(dl->_Path.Data,dl->_Path.Size, fillColorLeft, antiAliased);
    else if ((fillColorLeft & IM_COL32_A_MASK) != 0 || (fillColorRight & IM_COL32_A_MASK) != 0) ImDrawListAddConvexPolyFilledWithHorizontalGradient(dl, dl->_Path.Data, dl->_Path.Size, fillColorLeft, fillColorRight, antiAliased,minx,maxx);
    if ((strokeColor& IM_COL32_A_MASK)!= 0 && strokeThickness>0) dl->AddPolyline(dl->_Path.Data, dl->_Path.Size, strokeColor, strokeClosed, strokeThickness, antiAliased);
    dl->PathClear();
}
void ImDrawListAddRectWithHorizontalGradient(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, const ImU32 &fillColorLeft, const ImU32 &fillColoRight, const ImU32 &strokeColor, float rounding, int rounding_corners, float strokeThickness, bool antiAliased) {
    if (!dl || (((fillColorLeft & IM_COL32_A_MASK) == 0) && ((fillColoRight & IM_COL32_A_MASK) == 0) && ((strokeColor & IM_COL32_A_MASK) == 0)))  return;
    if (rounding==0.f || rounding_corners==0) {
        dl->AddRectFilledMultiColor(a,b,fillColorLeft,fillColoRight,fillColoRight,fillColorLeft); // Huge speedup!
        if ((strokeColor& IM_COL32_A_MASK)!= 0 && strokeThickness>0.f) {
            dl->PathRect(a, b, rounding, rounding_corners);
            dl->AddPolyline(dl->_Path.Data, dl->_Path.Size, strokeColor, true, strokeThickness, antiAliased);
            dl->PathClear();
        }
    }
    else    {
        dl->PathRect(a, b, rounding, rounding_corners);
        ImDrawListPathFillWithHorizontalGradientAndStroke(dl,fillColorLeft,fillColoRight,strokeColor,true,strokeThickness,antiAliased,a.x,b.x);
    }
}
void ImDrawListAddEllipseWithHorizontalGradient(ImDrawList *dl, const ImVec2 &centre, const ImVec2 &radii, const ImU32 &fillColorLeft, const ImU32 &fillColorRight, const ImU32 &strokeColor, int num_segments, float strokeThickness, bool antiAliased)   {
    if (!dl) return;
    const float a_max = IM_PI*2.0f * ((float)num_segments - 1.0f) / (float)num_segments;
    ImDrawListPathArcTo(dl,centre, radii, 0.0f, a_max, num_segments);
    ImDrawListPathFillWithHorizontalGradientAndStroke(dl,fillColorLeft,fillColorRight,strokeColor,true,strokeThickness,antiAliased,centre.y-radii.y,centre.y+radii.y);
}
void ImDrawListAddCircleWithHorizontalGradient(ImDrawList *dl, const ImVec2 &centre, float radius, const ImU32 &fillColorLeft, const ImU32 &fillColorRight, const ImU32 &strokeColor, int num_segments, float strokeThickness, bool antiAliased)   {
    if (!dl) return;
    const ImVec2 radii(radius,radius);
    const float a_max = IM_PI*2.0f * ((float)num_segments - 1.0f) / (float)num_segments;
    ImDrawListPathArcTo(dl,centre, radii, 0.0f, a_max, num_segments);
    ImDrawListPathFillWithHorizontalGradientAndStroke(dl,fillColorLeft,fillColorRight,strokeColor,true,strokeThickness,antiAliased,centre.y-radius,centre.y+radius);
}
void ImDrawListAddRectWithHorizontalGradient(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, const ImU32 &fillColor, float fillColorGradientDeltaIn0_05, const ImU32 &strokeColor, float rounding, int rounding_corners, float strokeThickness, bool antiAliased)   {
    ImU32 fillColorTop,fillColorBottom;GetVerticalGradientTopAndBottomColors(fillColor,fillColorGradientDeltaIn0_05,fillColorTop,fillColorBottom);
    ImDrawListAddRectWithHorizontalGradient(dl,a,b,fillColorTop,fillColorBottom,strokeColor,rounding,rounding_corners,strokeThickness,antiAliased);
}


#ifndef NO_IMGUIHELPER_VERTICAL_TEXT_METHODS
ImVec2 CalcVerticalTextSize(const char* text, const char* text_end, bool hide_text_after_double_hash, float wrap_width) {
    const ImVec2 rv = ImGui::CalcTextSize(text,text_end,hide_text_after_double_hash,wrap_width);
    return ImVec2(rv.y,rv.x);
}
void RenderTextVertical(const ImFont* font,ImDrawList* draw_list, float size, ImVec2 pos, ImU32 col, const ImVec4& clip_rect, const char* text_begin, const char* text_end, float wrap_width, bool cpu_fine_clip, bool rotateCCW) {
    if (!text_end) text_end = text_begin + strlen(text_begin);

    const float scale = size / font->FontSize;
    const float line_height = font->FontSize * scale;

    // Align to be pixel perfect
    pos.x = (float)(int)pos.x;// + (rotateCCW ? (font->FontSize-font->DisplayOffset.y) : 0);  // Not sure it's correct
    pos.y = (float)(int)pos.y + font->DisplayOffset.x;


    float x = pos.x;
    float y = pos.y;
    if (x > clip_rect.z)
        return;

    const bool word_wrap_enabled = (wrap_width > 0.0f);
    const char* word_wrap_eol = NULL;
    const float y_dir = rotateCCW ? -1.f : 1.f;

    // Skip non-visible lines
    const char* s = text_begin;
    if (!word_wrap_enabled && y + line_height < clip_rect.y)
        while (s < text_end && *s != '\n')  // Fast-forward to next line
            s++;

    // Reserve vertices for remaining worse case (over-reserving is useful and easily amortized)
    const int vtx_count_max = (int)(text_end - s) * 4;
    const int idx_count_max = (int)(text_end - s) * 6;
    const int idx_expected_size = draw_list->IdxBuffer.Size + idx_count_max;
    draw_list->PrimReserve(idx_count_max, vtx_count_max);

    ImDrawVert* vtx_write = draw_list->_VtxWritePtr;
    ImDrawIdx* idx_write = draw_list->_IdxWritePtr;
    unsigned int vtx_current_idx = draw_list->_VtxCurrentIdx;
    float x1=0.f,x2=0.f,y1=0.f,y2=0.f;

    while (s < text_end)
    {
        if (word_wrap_enabled)
        {
            // Calculate how far we can render. Requires two passes on the string data but keeps the code simple and not intrusive for what's essentially an uncommon feature.
            if (!word_wrap_eol)
            {
                word_wrap_eol = font->CalcWordWrapPositionA(scale, s, text_end, wrap_width - (y - pos.y));
                if (word_wrap_eol == s) // Wrap_width is too small to fit anything. Force displaying 1 character to minimize the height discontinuity.
                    word_wrap_eol++;    // +1 may not be a character start point in UTF-8 but it's ok because we use s >= word_wrap_eol below
            }

            if (s >= word_wrap_eol)
            {
                y = pos.y;
                x += line_height;
                word_wrap_eol = NULL;

                // Wrapping skips upcoming blanks
                while (s < text_end)
                {
                    const char c = *s;
                    if (ImCharIsSpace(c)) { s++; } else if (c == '\n') { s++; break; } else { break; }
                }
                continue;
            }
        }

        // Decode and advance source
        unsigned int c = (unsigned int)*s;
        if (c < 0x80)
        {
            s += 1;
        }
        else
        {
            s += ImTextCharFromUtf8(&c, s, text_end);
            if (c == 0)
                break;
        }

        if (c < 32)
        {
            if (c == '\n')
            {
                y = pos.y;
                x += line_height;

                if (x > clip_rect.z)
                    break;
                if (!word_wrap_enabled && x + line_height < clip_rect.x)
                    while (s < text_end && *s != '\n')  // Fast-forward to next line
                        s++;
                continue;
            }
            if (c == '\r')
                continue;
        }

        float char_width = 0.0f;
        if (const ImFont::Glyph* glyph = font->FindGlyph((unsigned short)c))
        {
            char_width = glyph->XAdvance * scale;
            //fprintf(stderr,"%c [%1.4f]\n",(unsigned char) glyph->Codepoint,char_width);

            // Arbitrarily assume that both space and tabs are empty glyphs as an optimization
            if (c != ' ' && c != '\t')
            {
                // We don't do a second finer clipping test on the Y axis as we've already skipped anything before clip_rect.y and exit once we pass clip_rect.w
                if (!rotateCCW)  {
                    x1 = x + (font->FontSize-glyph->Y1) * scale;
                    x2 = x + (font->FontSize-glyph->Y0) * scale;
                    y1 = y + glyph->X0 * scale;
                    y2 = y + glyph->X1 * scale;
                }
                else {
                    x1 = x + glyph->Y0 * scale;
                    x2 = x + glyph->Y1 * scale;
                    y1 = y - glyph->X1 * scale;
                    y2 = y - glyph->X0 * scale;
                }
                if (y1 <= clip_rect.w && y2 >= clip_rect.y)
                {
                    // Render a character
                    float u1 = glyph->U0;
                    float v1 = glyph->V0;
                    float u2 = glyph->U1;
                    float v2 = glyph->V1;

                    // CPU side clipping used to fit text in their frame when the frame is too small. Only does clipping for axis aligned quads.
                    if (cpu_fine_clip)
                    {
                        if (x1 < clip_rect.x)
                        {
                            u1 = u1 + (1.0f - (x2 - clip_rect.x) / (x2 - x1)) * (u2 - u1);
                            x1 = clip_rect.x;
                        }
                        if (y1 < clip_rect.y)
                        {
                            v1 = v1 + (1.0f - (y2 - clip_rect.y) / (y2 - y1)) * (v2 - v1);
                            y1 = clip_rect.y;
                        }
                        if (x2 > clip_rect.z)
                        {
                            u2 = u1 + ((clip_rect.z - x1) / (x2 - x1)) * (u2 - u1);
                            x2 = clip_rect.z;
                        }
                        if (y2 > clip_rect.w)
                        {
                            v2 = v1 + ((clip_rect.w - y1) / (y2 - y1)) * (v2 - v1);
                            y2 = clip_rect.w;
                        }
                        if (x1 >= x2)
                        {
                            y += char_width*y_dir;
                            continue;
                        }
                    }

                    // We are NOT calling PrimRectUV() here because non-inlined causes too much overhead in a debug build.
                    // Inlined here:
                    {
                        idx_write[0] = (ImDrawIdx)(vtx_current_idx); idx_write[1] = (ImDrawIdx)(vtx_current_idx+1); idx_write[2] = (ImDrawIdx)(vtx_current_idx+2);
                        idx_write[3] = (ImDrawIdx)(vtx_current_idx); idx_write[4] = (ImDrawIdx)(vtx_current_idx+2); idx_write[5] = (ImDrawIdx)(vtx_current_idx+3);
                        vtx_write[0].col = vtx_write[1].col = vtx_write[2].col = vtx_write[3].col = col;
                        vtx_write[0].pos.x = x1; vtx_write[0].pos.y = y1;
                        vtx_write[1].pos.x = x2; vtx_write[1].pos.y = y1;
                        vtx_write[2].pos.x = x2; vtx_write[2].pos.y = y2;
                        vtx_write[3].pos.x = x1; vtx_write[3].pos.y = y2;

                        if (rotateCCW) {
                            vtx_write[0].uv.x = u2; vtx_write[0].uv.y = v1;
                            vtx_write[1].uv.x = u2; vtx_write[1].uv.y = v2;
                            vtx_write[2].uv.x = u1; vtx_write[2].uv.y = v2;
                            vtx_write[3].uv.x = u1; vtx_write[3].uv.y = v1;
                        }
                        else {
                            vtx_write[0].uv.x = u1; vtx_write[0].uv.y = v2;
                            vtx_write[1].uv.x = u1; vtx_write[1].uv.y = v1;
                            vtx_write[2].uv.x = u2; vtx_write[2].uv.y = v1;
                            vtx_write[3].uv.x = u2; vtx_write[3].uv.y = v2;
                        }

                        vtx_write += 4;
                        vtx_current_idx += 4;
                        idx_write += 6;
                    }
                }
            }
        }

        y += char_width*y_dir;
    }

    // Give back unused vertices
    draw_list->VtxBuffer.resize((int)(vtx_write - draw_list->VtxBuffer.Data));
    draw_list->IdxBuffer.resize((int)(idx_write - draw_list->IdxBuffer.Data));
    draw_list->CmdBuffer[draw_list->CmdBuffer.Size-1].ElemCount -= (idx_expected_size - draw_list->IdxBuffer.Size);
    draw_list->_VtxWritePtr = vtx_write;
    draw_list->_IdxWritePtr = idx_write;
    draw_list->_VtxCurrentIdx = (unsigned int)draw_list->VtxBuffer.Size;
}
void AddTextVertical(ImDrawList* drawList,const ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end, float wrap_width, const ImVec4* cpu_fine_clip_rect,bool rotateCCW)    {
    if ((col & IM_COL32_A_MASK) == 0)
        return;

    if (text_end == NULL)
        text_end = text_begin + strlen(text_begin);
    if (text_begin == text_end)
        return;

    // Note: This is one of the few instance of breaking the encapsulation of ImDrawList, as we pull this from ImGui state, but it is just SO useful.
    // Might just move Font/FontSize to ImDrawList?
    if (font == NULL)
        font = GImGui->Font;
    if (font_size == 0.0f)
        font_size = GImGui->FontSize;

    IM_ASSERT(drawList && font->ContainerAtlas->TexID == drawList->_TextureIdStack.back());  // Use high-level ImGui::PushFont() or low-level ImDrawList::PushTextureId() to change font.

    ImVec4 clip_rect = drawList->_ClipRectStack.back();
    if (cpu_fine_clip_rect)
    {
        clip_rect.x = ImMax(clip_rect.x, cpu_fine_clip_rect->x);
        clip_rect.y = ImMax(clip_rect.y, cpu_fine_clip_rect->y);
        clip_rect.z = ImMin(clip_rect.z, cpu_fine_clip_rect->z);
        clip_rect.w = ImMin(clip_rect.w, cpu_fine_clip_rect->w);
    }
    RenderTextVertical(font, drawList, font_size, pos, col, clip_rect, text_begin, text_end, wrap_width, cpu_fine_clip_rect != NULL,rotateCCW);
}
void AddTextVertical(ImDrawList* drawList,const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end,bool rotateCCW)    {
    AddTextVertical(drawList,GImGui->Font, GImGui->FontSize, pos, col, text_begin, text_end,0.0f,NULL,rotateCCW);
}
void RenderTextVerticalClipped(const ImVec2& pos_min, const ImVec2& pos_max, const char* text, const char* text_end, const ImVec2* text_size_if_known, const ImVec2 &align, const ImVec2* clip_min, const ImVec2* clip_max, bool rotateCCW)    {
    // Hide anything after a '##' string
    const char* text_display_end = FindRenderedTextEnd(text, text_end);
    const int text_len = (int)(text_display_end - text);
    if (text_len == 0)
        return;

    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = GetCurrentWindow();

    // Perform CPU side clipping for single clipped element to avoid using scissor state
    ImVec2 pos = pos_min;
    const ImVec2 text_size = text_size_if_known ? *text_size_if_known : CalcVerticalTextSize(text, text_display_end, false, 0.0f);

    if (!clip_max) clip_max = &pos_max;
    bool need_clipping = (pos.x + text_size.x >= clip_max->x) || (pos.y + text_size.y >= clip_max->y);
    if (!clip_min) clip_min = &pos_min; else need_clipping |= (pos.x < clip_min->x) || (pos.y < clip_min->y);

    // Align
    /*if (align & ImGuiAlign_Center) pos.x = ImMax(pos.x, (pos.x + pos_max.x - text_size.x) * 0.5f);
    else if (align & ImGuiAlign_Right) pos.x = ImMax(pos.x, pos_max.x - text_size.x);
    if (align & ImGuiAlign_VCenter) pos.y = ImMax(pos.y, (pos.y + pos_max.y - text_size.y) * 0.5f);*/

    //if (align & ImGuiAlign_Center) pos.y = ImMax(pos.y, (pos.y + pos_max.y - text_size.y) * 0.5f);
    //else if (align & ImGuiAlign_Right) pos.y = ImMax(pos.y, pos_max.y - text_size.y);
    //if (align & ImGuiAlign_VCenter) pos.x = ImMax(pos.x, (pos.x + pos_max.x - text_size.x) * 0.5f);

    // Align whole block (we should defer that to the better rendering function
    if (align.x > 0.0f) pos.x = ImMax(pos.x, pos.x + (pos_max.x - pos.x - text_size.x) * align.x);
    if (align.y > 0.0f) pos.y = ImMax(pos.y, pos.y + (pos_max.y - pos.y - text_size.y) * align.y);


    if (rotateCCW) pos.y+=text_size.y;
    // Render
    if (need_clipping)
    {
        ImVec4 fine_clip_rect(clip_min->x, clip_min->y, clip_max->x, clip_max->y);
        AddTextVertical(window->DrawList,g.Font, g.FontSize, pos, GetColorU32(ImGuiCol_Text), text, text_display_end, 0.0f, &fine_clip_rect,rotateCCW);
    }
    else
    {
        AddTextVertical(window->DrawList,g.Font, g.FontSize, pos, GetColorU32(ImGuiCol_Text), text, text_display_end, 0.0f, NULL,rotateCCW);
    }
#   ifdef YES_IMGUIHELPER_LOGVERTICALTEXT   // LogRenderedText(...) is static in imgui.cpp and not exposed by imgui_internal.h: people that don't use IMGUI_INCLUDE_IMGUI_USER_INL to get here can't find it.
    if (g.LogEnabled) LogRenderedText(pos, text, text_display_end);
#   endif //YES_IMGUIHELPER_LOGVERTICALTEXT
}
#endif //NO_IMGUIHELPER_VERTICAL_TEXT_METHODS
#endif //NO_IMGUIHELPER_DRAW_METHODS

// These two methods are inspired by imguidock.cpp
void PutInBackground(const char* optionalRootWindowName)  {
    ImGuiWindow* w = optionalRootWindowName ? FindWindowByName(optionalRootWindowName) : GetCurrentWindow();
    if (!w) return;
    ImGuiContext& g = *GImGui;
    if (g.Windows[0] == w) return;
    const int isz = g.Windows.Size;
    for (int i = 0; i < isz; i++)  {
        if (g.Windows[i] == w)  {
            for (int j = i; j > 0; --j) g.Windows[j] = g.Windows[j-1];  // shifts [0,j-1] to [1,j]
            g.Windows[0] = w;
            break;
        }
    }
}
void PutInForeground(const char* optionalRootWindowName)  {
    ImGuiWindow* w = optionalRootWindowName ? FindWindowByName(optionalRootWindowName) : GetCurrentWindow();
    if (!w) return;
    ImGuiContext& g = *GImGui;
    const int iszMinusOne = g.Windows.Size - 1;
    if (iszMinusOne<0 || g.Windows[iszMinusOne] == w) return;
    for (int i = iszMinusOne; i >= 0; --i)  {
        if (g.Windows[i] == w)  {
            for (int j = i; j < iszMinusOne; j++) g.Windows[j] = g.Windows[j+1];  // shifts [i+1,iszMinusOne] to [i,iszMinusOne-1]
            g.Windows[iszMinusOne] = w;
            break;
        }
    }
}



} // namespace Imgui



#ifndef NO_IMGUIHELPER_SERIALIZATION
#include <stdio.h>  // FILE
namespace ImGuiHelper   {

static const char* FieldTypeNames[ImGui::FT_COUNT+1] = {"INT","UNSIGNED","FLOAT","DOUBLE","STRING","ENUM","BOOL","COLOR","TEXTLINE","CUSTOM","COUNT"};
static const char* FieldTypeFormats[ImGui::FT_COUNT]={"%d","%u","%f","%f","%s","%d","%d","%f","%s","%s"};
static const char* FieldTypeFormatsWithCustomPrecision[ImGui::FT_COUNT]={"%.*d","%*u","%.*f","%.*f","%*s","%*d","%*d","%.*f","%*s","%*s"};

#ifndef NO_IMGUIHELPER_SERIALIZATION_LOAD
void Deserializer::clear() {
    if (f_data) ImGui::MemFree(f_data);
    f_data = NULL;f_size=0;
}
bool Deserializer::loadFromFile(const char *filename) {
    clear();
    if (!filename) return false;
    FILE* f;
    if ((f = ImFileOpen(filename, "rt")) == NULL) return false;
    if (fseek(f, 0, SEEK_END))  {
        fclose(f);
        return false;
    }
    const long f_size_signed = ftell(f);
    if (f_size_signed == -1)    {
        fclose(f);
        return false;
    }
    f_size = (size_t)f_size_signed;
    if (fseek(f, 0, SEEK_SET))  {
        fclose(f);
        return false;
    }
    f_data = (char*)ImGui::MemAlloc(f_size+1);
    f_size = fread(f_data, 1, f_size, f); // Text conversion alter read size so let's not be fussy about return value
    fclose(f);
    if (f_size == 0)    {
        clear();
        return false;
    }
    f_data[f_size] = 0;
    ++f_size;
    return true;
}
bool Deserializer::allocate(size_t sizeToAllocate, const char *optionalTextToCopy, size_t optionalTextToCopySize)    {
    clear();
    if (sizeToAllocate==0) return false;
    f_size = sizeToAllocate;
    f_data = (char*)ImGui::MemAlloc(f_size);
    if (!f_data) {clear();return false;}
    if (optionalTextToCopy && optionalTextToCopySize>0) memcpy(f_data,optionalTextToCopy,optionalTextToCopySize>f_size ? f_size:optionalTextToCopySize);
    return true;
}
Deserializer::Deserializer(const char *filename) : f_data(NULL),f_size(0) {
    if (filename) loadFromFile(filename);
}
Deserializer::Deserializer(const char *text, size_t textSizeInBytes) : f_data(NULL),f_size(0) {
    allocate(textSizeInBytes,text,textSizeInBytes);
}

const char* Deserializer::parse(Deserializer::ParseCallback cb, void *userPtr, const char *optionalBufferStart) const {
    if (!cb || !f_data || f_size==0) return NULL;
    //------------------------------------------------
    // Parse file in memory
    char name[128];name[0]='\0';
    char typeName[32];char format[32]="";bool quitParsing = false;
    char charBuffer[sizeof(double)*10];void* voidBuffer = (void*) &charBuffer[0];
    static char textBuffer[2050];
    const char* varName = NULL;int numArrayElements = 0;FieldType ft = ImGui::FT_COUNT;
    const char* buf_end = f_data + f_size-1;
    for (const char* line_start = optionalBufferStart ? optionalBufferStart : f_data; line_start < buf_end; )
    {
        const char* line_end = line_start;
        while (line_end < buf_end && *line_end != '\n' && *line_end != '\r') line_end++;

        if (name[0]=='\0' && line_start[0] == '[' && line_end > line_start && line_end[-1] == ']')
        {
            ImFormatString(name, IM_ARRAYSIZE(name), "%.*s", (int)(line_end-line_start-2), line_start+1);
            //fprintf(stderr,"name: %s\n",name);  // dbg

            // Here we have something like: FLOAT-4:VariableName
            // We have to split into FLOAT 4 VariableName
            varName = NULL;numArrayElements = 0;ft = ImGui::FT_COUNT;format[0]='\0';
            const char* colonCh = strchr(name,':');
            const char* minusCh = strchr(name,'-');
            if (!colonCh) {
                fprintf(stderr,"ImGuiHelper::Deserializer::parse(...) warning (skipping line with no semicolon). name: %s\n",name);  // dbg
                name[0]='\0';
            }
            else {
                ptrdiff_t diff = 0,diff2 = 0;
                if (!minusCh || (minusCh-colonCh)>0)  {diff = (colonCh-name);numArrayElements=1;}
                else {
                    diff = (minusCh-name);
                    diff2 = colonCh-minusCh;
                    if (diff2>1 && diff2<7)    {
                        static char buff[8];
                        strncpy(&buff[0],(const char*) &minusCh[1],diff2);buff[diff2-1]='\0';
                        sscanf(buff,"%d",&numArrayElements);
                        //fprintf(stderr,"WARN: %s\n",buff);
                    }
                    else if (diff>0) numArrayElements = ((char)name[diff+1]-(char)'0');  // TODO: FT_STRING needs multibytes -> rewrite!
                }
                if (diff>0) {
                    const size_t len = (size_t)(diff>31?31:diff);
                    strncpy(typeName,name,len);typeName[len]='\0';

                    for (int t=0;t<=ImGui::FT_COUNT;t++) {
                        if (strcmp(typeName,FieldTypeNames[t])==0)  {
                            ft = (FieldType) t;break;
                        }
                    }
                    varName = ++colonCh;

                    const bool isTextOrCustomType = ft==ImGui::FT_STRING || ft==ImGui::FT_TEXTLINE  || ft==ImGui::FT_CUSTOM;
                    if (ft==ImGui::FT_COUNT || numArrayElements<1 || (numArrayElements>4 && !isTextOrCustomType))   {
                        fprintf(stderr,"ImGuiHelper::Deserializer::parse(...) Error (wrong type detected): line:%s type:%d numArrayElements:%d varName:%s typeName:%s\n",name,(int)ft,numArrayElements,varName,typeName);
                        varName=NULL;
                    }
                    else {

                        if (ft==ImGui::FT_STRING && varName && varName[0]!='\0')  {
                            //Process soon here, as the string can be multiline
                            line_start = ++line_end;
                            //--------------------------------------------------------
                            int cnt = 0;
                            while (line_end < buf_end && cnt++ < numArrayElements-1) ++line_end;
                            textBuffer[0]=textBuffer[2049]='\0';
                            const int maxLen = cnt>2049?2049:cnt;
                            strncpy(textBuffer,line_start,maxLen+1);
                            textBuffer[maxLen]='\0';
                            quitParsing = cb(ft,numArrayElements,(void*)textBuffer,varName,userPtr);
                            //fprintf(stderr,"Deserializer::parse(...) value:\"%s\" to type:%d numArrayElements:%d varName:%s maxLen:%d\n",textBuffer,(int)ft,numArrayElements,varName,maxLen);  // dbg


                            //else fprintf(stderr,"Deserializer::parse(...) Error converting value:\"%s\" to type:%d numArrayElements:%d varName:%s\n",line_start,(int)ft,numArrayElements,varName);  // dbg
                            //--------------------------------------------------------
                            ft = ImGui::FT_COUNT;name[0]='\0';varName=NULL; // mandatory                            

                        }
                        else if (!isTextOrCustomType) {
                            format[0]='\0';
                            for (int t=0;t<numArrayElements;t++) {
                                if (t>0) strcat(format," ");
                                strcat(format,FieldTypeFormats[ft]);
                            }
                            // DBG:
                            //fprintf(stderr,"Deserializer::parse(...) DBG: line:%s type:%d numArrayElements:%d varName:%s format:%s\n",name,(int)ft,numArrayElements,varName,format);  // dbg
                        }
                    }
                }
            }
        }
        else if (varName && varName[0]!='\0')
        {
            switch (ft) {
            case ImGui::FT_FLOAT:
            case ImGui::FT_COLOR:
            {
                float* p = (float*) voidBuffer;
                if ( (numArrayElements==1 && sscanf(line_start, format, p)==numArrayElements) ||
                     (numArrayElements==2 && sscanf(line_start, format, &p[0],&p[1])==numArrayElements) ||
                     (numArrayElements==3 && sscanf(line_start, format, &p[0],&p[1],&p[2])==numArrayElements) ||
                     (numArrayElements==4 && sscanf(line_start, format, &p[0],&p[1],&p[2],&p[3])==numArrayElements))
                     quitParsing = cb(ft,numArrayElements,voidBuffer,varName,userPtr);
                else fprintf(stderr,"Deserializer::parse(...) Error converting value:\"%s\" to type:%d numArrayElements:%d varName:%s\n",line_start,(int)ft,numArrayElements,varName);  // dbg
            }
            break;
            case ImGui::FT_DOUBLE:  {
                double* p = (double*) voidBuffer;
                if ( (numArrayElements==1 && sscanf(line_start, format, p)==numArrayElements) ||
                     (numArrayElements==2 && sscanf(line_start, format, &p[0],&p[1])==numArrayElements) ||
                     (numArrayElements==3 && sscanf(line_start, format, &p[0],&p[1],&p[2])==numArrayElements) ||
                     (numArrayElements==4 && sscanf(line_start, format, &p[0],&p[1],&p[2],&p[3])==numArrayElements))
                     quitParsing = cb(ft,numArrayElements,voidBuffer,varName,userPtr);
                else fprintf(stderr,"Deserializer::parse(...) Error converting value:\"%s\" to type:%d numArrayElements:%d varName:%s\n",line_start,(int)ft,numArrayElements,varName);  // dbg
            }
            break;
            case ImGui::FT_INT:
            case ImGui::FT_ENUM:
            {
                int* p = (int*) voidBuffer;
                if ( (numArrayElements==1 && sscanf(line_start, format, p)==numArrayElements) ||
                     (numArrayElements==2 && sscanf(line_start, format, &p[0],&p[1])==numArrayElements) ||
                     (numArrayElements==3 && sscanf(line_start, format, &p[0],&p[1],&p[2])==numArrayElements) ||
                     (numArrayElements==4 && sscanf(line_start, format, &p[0],&p[1],&p[2],&p[3])==numArrayElements))
                     quitParsing = cb(ft,numArrayElements,voidBuffer,varName,userPtr);
                else fprintf(stderr,"Deserializer::parse(...) Error converting value:\"%s\" to type:%d numArrayElements:%d varName:%s\n",line_start,(int)ft,numArrayElements,varName);  // dbg
            }
            break;
            case ImGui::FT_BOOL:
            {
                bool* p = (bool*) voidBuffer;
                int tmp[4];
                if ( (numArrayElements==1 && sscanf(line_start, format, &tmp[0])==numArrayElements) ||
                     (numArrayElements==2 && sscanf(line_start, format, &tmp[0],&tmp[1])==numArrayElements) ||
                     (numArrayElements==3 && sscanf(line_start, format, &tmp[0],&tmp[1],&tmp[2])==numArrayElements) ||
                     (numArrayElements==4 && sscanf(line_start, format, &tmp[0],&tmp[1],&tmp[2],&tmp[3])==numArrayElements))    {
                     for (int i=0;i<numArrayElements;i++) p[i] = tmp[i];
                     quitParsing = cb(ft,numArrayElements,voidBuffer,varName,userPtr);quitParsing = cb(ft,numArrayElements,voidBuffer,varName,userPtr);
                }
                else fprintf(stderr,"Deserializer::parse(...) Error converting value:\"%s\" to type:%d numArrayElements:%d varName:%s\n",line_start,(int)ft,numArrayElements,varName);  // dbg
            }
            break;
            case ImGui::FT_UNSIGNED:  {
                unsigned* p = (unsigned*) voidBuffer;
                if ( (numArrayElements==1 && sscanf(line_start, format, p)==numArrayElements) ||
                     (numArrayElements==2 && sscanf(line_start, format, &p[0],&p[1])==numArrayElements) ||
                     (numArrayElements==3 && sscanf(line_start, format, &p[0],&p[1],&p[2])==numArrayElements) ||
                     (numArrayElements==4 && sscanf(line_start, format, &p[0],&p[1],&p[2],&p[3])==numArrayElements))
                     quitParsing = cb(ft,numArrayElements,voidBuffer,varName,userPtr);
                else fprintf(stderr,"Deserializer::parse(...) Error converting value:\"%s\" to type:%d numArrayElements:%d varName:%s\n",line_start,(int)ft,numArrayElements,varName);  // dbg
            }
            break;
            case ImGui::FT_CUSTOM:
            case ImGui::FT_TEXTLINE:
            {
                // A similiar code can be used to parse "numArrayElements" line of text
                for (int i=0;i<numArrayElements;i++)    {
                    textBuffer[0]=textBuffer[2049]='\0';
                    const int maxLen = (line_end-line_start)>2049?2049:(line_end-line_start);
                    if (maxLen<=0) break;
                    strncpy(textBuffer,line_start,maxLen+1);textBuffer[maxLen]='\0';
                    quitParsing = cb(ft,i,(void*)textBuffer,varName,userPtr);

                    //fprintf(stderr,"%d) \"%s\"\n",i,textBuffer);  // Dbg

                    if (quitParsing) break;
                    line_start = line_end+1;
                    line_end = line_start;
                    if (line_end == buf_end) break;
                    while (line_end < buf_end && *line_end != '\n' && *line_end != '\r') line_end++;
                }
            }
            break;
            default:
            fprintf(stderr,"Deserializer::parse(...) Warning missing value type:\"%s\" to type:%d numArrayElements:%d varName:%s\n",line_start,(int)ft,numArrayElements,varName);  // dbg
            break;
            }
            //---------------------------------------------------------------------------------
            name[0]='\0';varName=NULL; // mandatory
        }

        line_start = line_end+1;

        if (quitParsing) return line_start;
    }

    //------------------------------------------------
    return buf_end;
}

bool GetFileContent(const char *filePath, ImVector<char> &contentOut, bool clearContentOutBeforeUsage, const char *modes, bool appendTrailingZeroIfModesIsNotBinary)   {
    ImVector<char>& f_data = contentOut;
    if (clearContentOutBeforeUsage) f_data.clear();
//----------------------------------------------------
    if (!filePath) return false;
    const bool appendTrailingZero = appendTrailingZeroIfModesIsNotBinary && modes && strlen(modes)>0 && modes[strlen(modes)-1]!='b';
    FILE* f;
    if ((f = ImFileOpen(filePath, modes)) == NULL) return false;
    if (fseek(f, 0, SEEK_END))  {
        fclose(f);
        return false;
    }
    const long f_size_signed = ftell(f);
    if (f_size_signed == -1)    {
        fclose(f);
        return false;
    }
    size_t f_size = (size_t)f_size_signed;
    if (fseek(f, 0, SEEK_SET))  {
        fclose(f);
        return false;
    }
    f_data.resize(f_size+(appendTrailingZero?1:0));
    const size_t f_size_read = f_size>0 ? fread(&f_data[0], 1, f_size, f) : 0;
    fclose(f);
    if (f_size_read == 0 || f_size_read!=f_size)    return false;
    if (appendTrailingZero) f_data[f_size] = '\0';
//----------------------------------------------------
    return true;
}
bool FileExists(const char *filePath)   {
    if (!filePath || strlen(filePath)==0) return false;
    FILE* f = ImFileOpen(filePath, "rb");
    if (!f) return false;
    fclose(f);f=NULL;
    return true;
}
#endif //NO_IMGUIHELPER_SERIALIZATION_LOAD
#ifndef NO_IMGUIHELPER_SERIALIZATION_SAVE
bool SetFileContent(const char *filePath, const unsigned char* content, int contentSize,const char* modes)	{
    if (!filePath || !content) return false;
    FILE* f;
    if ((f = ImFileOpen(filePath, modes)) == NULL) return false;
    fwrite(content, contentSize, 1, f);
    fclose(f);f=NULL;
    return true;
}

class ISerializable {
public:
    ISerializable() {}
    virtual ~ISerializable() {}
    virtual void close()=0;
    virtual bool isValid() const=0;
    virtual int print(const char* fmt, ...)=0;
    virtual int getTypeID() const=0;
};
class SerializeToFile : public ISerializable {
public:
    SerializeToFile(const char* filename) : f(NULL) {
        saveToFile(filename);
    }
    SerializeToFile() : f(NULL) {}
    ~SerializeToFile() {close();}
    bool saveToFile(const char* filename) {
        close();
        f = ImFileOpen(filename,"w");
        return (f);
    }
    void close() {if (f) fclose(f);f=NULL;}
    bool isValid() const {return (f);}
    int print(const char* fmt, ...) {
        va_list args;va_start(args, fmt);
        const int rv = vfprintf(f,fmt,args);
        va_end(args);
        return rv;
    }
    int getTypeID() const {return 0;}
protected:
    FILE* f;
};
class SerializeToBuffer : public ISerializable {
public:
    SerializeToBuffer(int initialCapacity=2048) {b.reserve(initialCapacity);b.resize(1);b[0]='\0';}
    ~SerializeToBuffer() {close();}
    bool saveToFile(const char* filename) {
        if (!isValid()) return false;
        return SetFileContent(filename,(unsigned char*)&b[0],b.size(),"w");
    }
    void close() {b.clear();ImVector<char> o;b.swap(o);b.resize(1);b[0]='\0';}
    bool isValid() const {return b.size()>0;}
    int print(const char* fmt, ...) {
        va_list args,args2;
        va_start(args, fmt);
        va_copy(args2,args);                                    // since C99 (MANDATORY! otherwise we must reuse va_start(args2,fmt): slow)
        const int additionalSize = vsnprintf(NULL,0,fmt,args);  // since C99
        va_end(args);
        //IM_ASSERT(additionalSize>0);

        const int startSz = b.size();
        b.resize(startSz+additionalSize);
        const int rv = vsprintf(&b[startSz-1],fmt,args2);
        va_end(args2);
        //IM_ASSERT(additionalSize==rv);
        //IM_ASSERT(v[startSz+additionalSize-1]=='\0');

        return rv;
    }
    inline const char* getBuffer() const {return b.size()>0 ? &b[0] : NULL;}
    inline int getBufferSize() const {return b.size();}
    int getTypeID() const {return 1;}
protected:
    ImVector<char> b;
};
const char* Serializer::getBuffer() const   {
    return (f && f->getTypeID()==1 && f->isValid()) ? static_cast<SerializeToBuffer*>(f)->getBuffer() : NULL;
}
int Serializer::getBufferSize() const {
    return (f && f->getTypeID()==1 && f->isValid()) ? static_cast<SerializeToBuffer*>(f)->getBufferSize() : 0;
}
bool Serializer::WriteBufferToFile(const char* filename,const char* buffer,int bufferSize)   {
    if (!buffer) return false;
    FILE* f = ImFileOpen(filename,"w");
    if (!f) return false;
    fwrite((void*) buffer,bufferSize,1,f);
    fclose(f);
    return true;
}

void Serializer::clear() {if (f) {f->close();}}
Serializer::Serializer(const char *filename) {
    f=(SerializeToFile*) ImGui::MemAlloc(sizeof(SerializeToFile));
    IM_PLACEMENT_NEW((SerializeToFile*)f) SerializeToFile(filename);
}
Serializer::Serializer(int memoryBufferCapacity) {
    f=(SerializeToBuffer*) ImGui::MemAlloc(sizeof(SerializeToBuffer));
    IM_PLACEMENT_NEW((SerializeToBuffer*)f) SerializeToBuffer(memoryBufferCapacity);
}
Serializer::~Serializer() {
    if (f) {
        f->~ISerializable();
        ImGui::MemFree(f);
        f=NULL;
    }
}
template <typename T> inline static bool SaveTemplate(ISerializable* f,FieldType ft, const T* pValue, const char* name, int numArrayElements=1, int prec=-1)   {
    if (!f || ft==ImGui::FT_COUNT  || ft==ImGui::FT_CUSTOM || numArrayElements<0 || numArrayElements>4 || !pValue || !name || name[0]=='\0') return false;
    // name
    f->print( "[%s",FieldTypeNames[ft]);
    if (numArrayElements==0) numArrayElements=1;
    if (numArrayElements>1) f->print( "-%d",numArrayElements);
    f->print( ":%s]\n",name);
    // value
    const char* precision = FieldTypeFormatsWithCustomPrecision[ft];
    for (int t=0;t<numArrayElements;t++) {
        if (t>0) f->print(" ");
        f->print(precision,prec,pValue[t]);
    }
    f->print("\n\n");
    return true;
}
bool Serializer::save(FieldType ft, const float* pValue, const char* name, int numArrayElements,  int prec)   {
    IM_ASSERT(ft==ImGui::FT_FLOAT || ft==ImGui::FT_COLOR);
    return SaveTemplate<float>(f,ft,pValue,name,numArrayElements,prec);
}
bool Serializer::save(const double* pValue,const char* name,int numArrayElements, int prec)   {
    return SaveTemplate<double>(f,ImGui::FT_DOUBLE,pValue,name,numArrayElements,prec);
}
bool Serializer::save(const bool* pValue,const char* name,int numArrayElements)   {
    if (!pValue || numArrayElements<0 || numArrayElements>4) return false;
    static int tmp[4];
    for (int i=0;i<numArrayElements;i++) tmp[i] = pValue[i] ? 1 : 0;
    return SaveTemplate<int>(f,ImGui::FT_BOOL,tmp,name,numArrayElements);
}
bool Serializer::save(FieldType ft,const int* pValue,const char* name,int numArrayElements, int prec) {
    IM_ASSERT(ft==ImGui::FT_INT || ft==ImGui::FT_BOOL || ft==ImGui::FT_ENUM);
    if (prec==0) prec=-1;
    return SaveTemplate<int>(f,ft,pValue,name,numArrayElements,prec);
}
bool Serializer::save(const unsigned* pValue,const char* name,int numArrayElements, int prec) {
    if (prec==0) prec=-1;
    return SaveTemplate<unsigned>(f,ImGui::FT_UNSIGNED,pValue,name,numArrayElements,prec);
}
bool Serializer::save(const char* pValue,const char* name,int pValueSize)    {
    FieldType ft = ImGui::FT_STRING;
    int numArrayElements = pValueSize;
    if (!f || ft==ImGui::FT_COUNT || !pValue || !name || name[0]=='\0') return false;
    numArrayElements = pValueSize;
    pValueSize=(int)strlen(pValue);if (numArrayElements>pValueSize || numArrayElements<=0) numArrayElements=pValueSize;
    if (numArrayElements<0) numArrayElements=0;

    // name
    f->print( "[%s",FieldTypeNames[ft]);
    if (numArrayElements==0) numArrayElements=1;
    if (numArrayElements>1) f->print( "-%d",numArrayElements);
    f->print( ":%s]\n",name);
    // value
    f->print("%s\n\n",pValue);
    return true;
}
bool Serializer::saveTextLines(const char* pValue,const char* name)   {
    FieldType ft = ImGui::FT_TEXTLINE;
    if (!f || ft==ImGui::FT_COUNT || !pValue || !name || name[0]=='\0') return false;
    const char *tmp;const char *start = pValue;
    int left = strlen(pValue);int numArrayElements =0;  // numLines
    bool endsWithNewLine = pValue[left-1]=='\n';
    while ((tmp=strchr(start, '\n'))) {
        ++numArrayElements;
        left-=tmp-start-1;
        start = ++tmp;  // to skip '\n'
    }
    if (left>0) ++numArrayElements;
    if (numArrayElements==0) return false;

    // name
    f->print( "[%s",FieldTypeNames[ft]);
    if (numArrayElements==0) numArrayElements=1;
    if (numArrayElements>1) f->print( "-%d",numArrayElements);
    f->print( ":%s]\n",name);
    // value
    f->print("%s",pValue);
    if (!endsWithNewLine)  f->print("\n");
    f->print("\n");
    return true;
}
bool Serializer::saveTextLines(int numValues,bool (*items_getter)(void* data, int idx, const char** out_text),void* data,const char* name)  {
    FieldType ft = ImGui::FT_TEXTLINE;
    if (!items_getter || !f || ft==ImGui::FT_COUNT || numValues<=0 || !name || name[0]=='\0') return false;
    int numArrayElements =numValues;  // numLines

    // name
    f->print( "[%s",FieldTypeNames[ft]);
    if (numArrayElements==0) numArrayElements=1;
    if (numArrayElements>1) f->print( "-%d",numArrayElements);
    f->print( ":%s]\n",name);

    // value
    const char* text=NULL;int len=0;
    for (int i=0;i<numArrayElements;i++)    {
        if (items_getter(data,i,&text)) {
            f->print("%s",text);
            if (len<=0 || text[len-1]!='\n')  f->print("\n");
        }
        else f->print("\n");
    }
    f->print("\n");
    return true;
}
bool Serializer::saveCustomFieldTypeHeader(const char* name, int numTextLines) {
    // name
    f->print( "[%s",FieldTypeNames[ImGui::FT_CUSTOM]);
    if (numTextLines==0) numTextLines=1;
    if (numTextLines>1) f->print( "-%d",numTextLines);
    f->print( ":%s]\n",name);
    return true;
}

#endif //NO_IMGUIHELPER_SERIALIZATION_SAVE

void StringSet(char *&destText, const char *text, bool allowNullDestText) {
    if (destText) {ImGui::MemFree(destText);destText=NULL;}
    const char e = '\0';
    if (!text && !allowNullDestText) text=&e;
    if (text)  {
        const int sz = strlen(text);
        destText = (char*) ImGui::MemAlloc(sz+1);strcpy(destText,text);
    }
}
void StringAppend(char *&destText, const char *textToAppend, bool allowNullDestText, bool prependLineFeedIfDestTextIsNotEmpty, bool mustAppendLineFeed) {
    const int textToAppendSz = textToAppend ? strlen(textToAppend) : 0;
    if (textToAppendSz==0) {
        if (!destText && !allowNullDestText) {destText = (char*) ImGui::MemAlloc(1);strcpy(destText,"");}
        return;
    }
    const int destTextSz = destText ? strlen(destText) : 0;
    const bool mustPrependLF = prependLineFeedIfDestTextIsNotEmpty && (destTextSz>0);
    const bool mustAppendLF = mustAppendLineFeed;// && (destText);
    const int totalTextSz = textToAppendSz + destTextSz + (mustPrependLF?1:0) + (mustAppendLF?1:0);
    ImVector<char> totalText;totalText.resize(totalTextSz+1);
    totalText[0]='\0';
    if (destText) {
        strcpy(&totalText[0],destText);
        ImGui::MemFree(destText);destText=NULL;
    }
    if (mustPrependLF) strcat(&totalText[0],"\n");
    strcat(&totalText[0],textToAppend);
    if (mustAppendLF) strcat(&totalText[0],"\n");
    destText = (char*) ImGui::MemAlloc(totalTextSz+1);strcpy(destText,&totalText[0]);
}
int StringAppend(ImVector<char>& v,const char* fmt, ...) {
    IM_ASSERT(v.size()>0 && v[v.size()-1]=='\0');
    va_list args,args2;

    va_start(args, fmt);
    va_copy(args2,args);                                    // since C99 (MANDATORY! otherwise we must reuse va_start(args2,fmt): slow)
    const int additionalSize = vsnprintf(NULL,0,fmt,args);  // since C99
    va_end(args);

    const int startSz = v.size();
    v.resize(startSz+additionalSize);
    const int rv = vsprintf(&v[startSz-1],fmt,args2);
    va_end(args2);

    return rv;
}


} //namespace ImGuiHelper
#endif //NO_IMGUIHELPER_SERIALIZATION


#ifdef IMGUI_USE_ZLIB	// requires linking to library -lZlib
#include <zlib.h>

namespace ImGui {

#ifndef NO_IMGUIHELPER_SERIALIZATION
#ifndef NO_IMGUIHELPER_SERIALIZATION_LOAD
bool GzDecompressFromFile(const char* filePath,ImVector<char>& rv,bool clearRvBeforeUsage)   {
    if (clearRvBeforeUsage) rv.clear();
    ImVector<char> f_data;
    if (!ImGuiHelper::GetFileContent(filePath,f_data,true,"rb",false)) return false;
    //----------------------------------------------------
    return GzDecompressFromMemory(&f_data[0],f_data.size(),rv,clearRvBeforeUsage);
    //----------------------------------------------------
}
#   ifdef YES_IMGUISTRINGIFIER
bool GzBase64DecompressFromFile(const char* filePath,ImVector<char>& rv)    {
    ImVector<char> f_data;
    if (!ImGuiHelper::GetFileContent(filePath,f_data,true,"r",true)) return false;
    return ImGui::GzBase64DecompressFromMemory(&f_data[0],rv);
}
bool GzBase85DecompressFromFile(const char* filePath,ImVector<char>& rv)    {
    ImVector<char> f_data;
    if (!ImGuiHelper::GetFileContent(filePath,f_data,true,"r",true)) return false;
    return ImGui::GzBase85DecompressFromMemory(&f_data[0],rv);
}
#   endif //#YES_IMGUISTRINGIFIER
#endif //NO_IMGUIHELPER_SERIALIZATION_LOAD
#endif //NO_IMGUIHELPER_SERIALIZATION

bool GzDecompressFromMemory(const char* memoryBuffer,int memoryBufferSize,ImVector<char>& rv,bool clearRvBeforeUsage)    {
    if (clearRvBeforeUsage) rv.clear();
    const int startRv = rv.size();

    if (memoryBufferSize == 0  || !memoryBuffer) return false;
    const int memoryChunk = memoryBufferSize > (16*1024) ? (16*1024) : memoryBufferSize;
    rv.resize(startRv+memoryChunk);  // we start using the memoryChunk length

    z_stream myZStream;
    myZStream.next_in = (Bytef *) memoryBuffer;
    myZStream.avail_in = memoryBufferSize;
    myZStream.total_out = 0;
    myZStream.zalloc = Z_NULL;
    myZStream.zfree = Z_NULL;

    bool done = false;
    if (inflateInit2(&myZStream, (16+MAX_WBITS)) == Z_OK) {
        int err = Z_OK;
        while (!done) {
            if (myZStream.total_out >= (uLong)(rv.size()-startRv)) rv.resize(rv.size()+memoryChunk);    // not enough space: we add the memoryChunk each step

            myZStream.next_out = (Bytef *) (&rv[startRv] + myZStream.total_out);
            myZStream.avail_out = rv.size() - startRv - myZStream.total_out;

            if ((err = inflate (&myZStream, Z_SYNC_FLUSH))==Z_STREAM_END) done = true;
            else if (err != Z_OK)  break;
        }
        if ((err=inflateEnd(&myZStream))!= Z_OK) done = false;
    }
    rv.resize(startRv+(done ? myZStream.total_out : 0));

    return done;
}
bool GzCompressFromMemory(const char* memoryBuffer,int memoryBufferSize,ImVector<char>& rv,bool clearRvBeforeUsage)  {
    if (clearRvBeforeUsage) rv.clear();
    const int startRv = rv.size();

    if (memoryBufferSize == 0  || !memoryBuffer) return false;
    const int memoryChunk = memoryBufferSize/3 > (16*1024) ? (16*1024) : memoryBufferSize/3;
    rv.resize(startRv+memoryChunk);  // we start using the memoryChunk length

    z_stream myZStream;
    myZStream.next_in =  (Bytef *) memoryBuffer;
    myZStream.avail_in = memoryBufferSize;
    myZStream.total_out = 0;
    myZStream.zalloc = Z_NULL;
    myZStream.zfree = Z_NULL;

    bool done = false;
    if (deflateInit2(&myZStream,Z_BEST_COMPRESSION,Z_DEFLATED,(16+MAX_WBITS),8,Z_DEFAULT_STRATEGY) == Z_OK) {
        int err = Z_OK;
        while (!done) {
            if (myZStream.total_out >= (uLong)(rv.size()-startRv)) rv.resize(rv.size()+memoryChunk);    // not enough space: we add the full memoryChunk each step

            myZStream.next_out = (Bytef *) (&rv[startRv] + myZStream.total_out);
            myZStream.avail_out = rv.size() - startRv - myZStream.total_out;

            if ((err = deflate (&myZStream, Z_FINISH))==Z_STREAM_END) done = true;
            else if (err != Z_OK)  break;
        }
        if ((err=deflateEnd(&myZStream))!= Z_OK) done=false;
    }
    rv.resize(startRv+(done ? myZStream.total_out : 0));

    return done;
}
#   ifdef YES_IMGUISTRINGIFIER
bool GzBase64DecompressFromMemory(const char* input,ImVector<char>& rv) {
    rv.clear();ImVector<char> v;
    if (ImGui::Base64Decode(input,v)) return false;
    if (v.size()==0) return false;
    return GzDecompressFromMemory(&v[0],v.size(),rv);
}
bool GzBase85DecompressFromMemory(const char* input,ImVector<char>& rv) {
    rv.clear();ImVector<char> v;
    if (ImGui::Base85Decode(input,v)) return false;
    if (v.size()==0) return false;
    return GzDecompressFromMemory(&v[0],v.size(),rv);
}
bool GzBase64CompressFromMemory(const char* input,int inputSize,ImVector<char>& output,bool stringifiedMode,int numCharsPerLineInStringifiedMode)   {
    output.clear();ImVector<char> output1;
    if (!ImGui::GzCompressFromMemory(input,inputSize,output1)) return false;
    return ImGui::Base64Encode(&output1[0],output1.size(),output,stringifiedMode,numCharsPerLineInStringifiedMode);
}
bool GzBase85CompressFromMemory(const char* input,int inputSize,ImVector<char>& output,bool stringifiedMode,int numCharsPerLineInStringifiedMode) {
    output.clear();ImVector<char> output1;
    if (!ImGui::GzCompressFromMemory(input,inputSize,output1)) return false;
    return ImGui::Base85Encode(&output1[0],output1.size(),output,stringifiedMode,numCharsPerLineInStringifiedMode);
}
#   endif //#YES_IMGUISTRINGIFIER

} // namespace ImGui
#endif //IMGUI_USE_ZLIB

#   ifdef YES_IMGUIBZ2
//#include "../imguiyesaddons/imguibz2.h"   // This should be already included
namespace ImGui {
// Two methods that fill rv and return true on success
#       ifndef NO_IMGUIHELPER_SERIALIZATION
#           ifndef NO_IMGUIHELPER_SERIALIZATION_LOAD
bool Bz2DecompressFromFile(const char* filePath,ImVector<char>& rv,bool clearRvBeforeUsage) {
    if (clearRvBeforeUsage) rv.clear();
    ImVector<char> f_data;
    if (!ImGuiHelper::GetFileContent(filePath,f_data,true,"rb",false)) return false;
    //----------------------------------------------------
    return ImGui::Bz2DecompressFromMemory(&f_data[0],f_data.size(),rv,clearRvBeforeUsage);
    //----------------------------------------------------
}
#   ifdef YES_IMGUISTRINGIFIER
bool Bz2Base64DecompressFromFile(const char* filePath,ImVector<char>& rv)   {
    ImVector<char> f_data;
    if (!ImGuiHelper::GetFileContent(filePath,f_data,true,"r",true)) return false;
    return ImGui::Bz2Base64Decode(&f_data[0],rv);
}
bool Bz2Base85DecompressFromFile(const char* filePath, ImVector<char>& rv)   {
    ImVector<char> f_data;
    if (!ImGuiHelper::GetFileContent(filePath,f_data,true,"r",true)) return false;
    return ImGui::Bz2Base85Decode(&f_data[0],rv);
}
#   endif //#YES_IMGUISTRINGIFIER
#           endif //NO_IMGUIHELPER_SERIALIZATION_LOAD
#       endif //NO_IMGUIHELPER_SERIALIZATION
} // namespace ImGui
#   endif //YES_IMGUIBZ2

#   ifdef YES_IMGUISTRINGIFIER
namespace ImGui {
// Two methods that fill rv and return true on success
#       ifndef NO_IMGUIHELPER_SERIALIZATION
#           ifndef NO_IMGUIHELPER_SERIALIZATION_LOAD
bool Base64DecodeFromFile(const char* filePath,ImVector<char>& rv)  {
    ImVector<char> f_data;
    if (!ImGuiHelper::GetFileContent(filePath,f_data,true,"r",true)) return false;
    return ImGui::Base64Decode(&f_data[0],rv);
}
bool Base85DecodeFromFile(const char* filePath,ImVector<char>& rv)  {
    ImVector<char> f_data;
    if (!ImGuiHelper::GetFileContent(filePath,f_data,true,"r",true)) return false;
    return ImGui::Base85Decode(&f_data[0],rv);
}
#           endif //NO_IMGUIHELPER_SERIALIZATION_LOAD
#       endif //NO_IMGUIHELPER_SERIALIZATION
} // namespace ImGui
#   endif //YES_IMGUISTRINGIFIER
