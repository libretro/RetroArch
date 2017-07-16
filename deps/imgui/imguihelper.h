#ifndef IMGUIHELPER_H_
#define IMGUIHELPER_H_

#ifndef IMGUI_API
#include <imgui.h>
#endif //IMGUI_API


namespace ImGui {

// Experimental: tested on Ubuntu only. Should work with urls, folders and files.
bool OpenWithDefaultApplication(const char* url,bool exploreModeForWindowsOS=false);

void CloseAllPopupMenus();  // Never Tested

bool IsItemActiveLastFrame();
bool IsItemJustReleased();


#ifndef NO_IMGUIHELPER_FONT_METHODS
const ImFont* GetFont(int fntIndex);
void PushFont(int fntIndex);    // using the index of the font instead of a ImFont* is easier (you can set up an enum).
void TextColoredV(int fntIndex,const ImVec4& col, const char* fmt, va_list args);
void TextColored(int fntIndex,const ImVec4& col, const char* fmt, ...) IM_PRINTFARGS(3);
void TextV(int fntIndex,const char* fmt, va_list args);
void Text(int fntIndex,const char* fmt, ...) IM_PRINTFARGS(2);

// Handy if we want to use ImGui::Image(...) or ImGui::ImageButton(...) with a glyph
bool GetTexCoordsFromGlyph(unsigned short glyph,ImVec2& uv0,ImVec2& uv1);
// Returns the height of the main menu based on the current font and style
float CalcMainMenuHeight();
#endif //NO_IMGUIHELPER_FONT_METHODS

#ifndef NO_IMGUIHELPER_DRAW_METHODS
// Extensions to ImDrawList
void ImDrawListAddConvexPolyFilledWithVerticalGradient(ImDrawList* dl, const ImVec2* points, const int points_count, ImU32 colTop, ImU32 colBot, bool anti_aliased, float miny=-1.f, float maxy=-1.f);
void ImDrawListPathFillWithVerticalGradientAndStroke(ImDrawList* dl, const ImU32& fillColorTop, const ImU32& fillColorBottom, const ImU32& strokeColor, bool strokeClosed=false, float strokeThickness = 1.0f, bool antiAliased = true, float miny=-1.f, float maxy=-1.f);
void ImDrawListPathFillAndStroke(ImDrawList* dl,const ImU32& fillColor,const ImU32& strokeColor,bool strokeClosed=false, float strokeThickness = 1.0f, bool antiAliased = true);
void ImDrawListAddRect(ImDrawList* dl,const ImVec2& a, const ImVec2& b,const ImU32& fillColor,const ImU32& strokeColor,float rounding = 0.0f, int rounding_corners = ~0,float strokeThickness = 1.0f,bool antiAliased = true);
void ImDrawListAddRectWithVerticalGradient(ImDrawList* dl,const ImVec2& a, const ImVec2& b,const ImU32& fillColorTop,const ImU32& fillColorBottom,const ImU32& strokeColor,float rounding = 0.0f, int rounding_corners = ~0,float strokeThickness = 1.0f,bool antiAliased = true);
void ImDrawListAddRectWithVerticalGradient(ImDrawList* dl,const ImVec2& a, const ImVec2& b,const ImU32& fillColor,float fillColorGradientDeltaIn0_05,const ImU32& strokeColor,float rounding = 0.0f, int rounding_corners = ~0,float strokeThickness = 1.0f,bool antiAliased = true);
void ImDrawListPathArcTo(ImDrawList* dl,const ImVec2& centre,const ImVec2& radii, float amin, float amax, int num_segments = 10);
void ImDrawListAddEllipse(ImDrawList* dl,const ImVec2& centre, const ImVec2& radii,const ImU32& fillColor,const ImU32& strokeColor,int num_segments = 12,float strokeThickness = 1.f,bool antiAliased = true);
void ImDrawListAddEllipseWithVerticalGradient(ImDrawList* dl,const ImVec2& centre, const ImVec2& radii,const ImU32& fillColorTop,const ImU32& fillColorBottom,const ImU32& strokeColor,int num_segments = 12,float strokeThickness = 1.f,bool antiAliased = true);
void ImDrawListAddCircle(ImDrawList* dl,const ImVec2& centre, float radius,const ImU32& fillColor,const ImU32& strokeColor,int num_segments = 12,float strokeThickness = 1.f,bool antiAliased = true);
void ImDrawListAddCircleWithVerticalGradient(ImDrawList* dl,const ImVec2& centre, float radius,const ImU32& fillColorTop,const ImU32& fillColorBottom,const ImU32& strokeColor,int num_segments = 12,float strokeThickness = 1.f,bool antiAliased = true);
// Overload of ImDrawList::addPolyLine(...) that takes offset and scale:
void ImDrawListAddPolyLine(ImDrawList *dl,const ImVec2* polyPoints,int numPolyPoints,ImU32 strokeColor=IM_COL32_WHITE,float strokeThickness=1.f,bool strokeClosed=false, const ImVec2 &offset=ImVec2(0,0), const ImVec2& scale=ImVec2(1,1),bool antiAliased=false);


void ImDrawListAddConvexPolyFilledWithHorizontalGradient(ImDrawList *dl, const ImVec2 *points, const int points_count, ImU32 colLeft, ImU32 colRight, bool anti_aliased,float minx=-1.f,float maxx=-1.f);
void ImDrawListPathFillWithHorizontalGradientAndStroke(ImDrawList *dl, const ImU32 &fillColorLeft, const ImU32 &fillColorRight, const ImU32 &strokeColor, bool strokeClosed=false, float strokeThickness = 1.0f, bool antiAliased = true,float minx=-1.f,float maxx=-1.f);
void ImDrawListAddRectWithHorizontalGradient(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, const ImU32 &fillColorLeft, const ImU32 &fillColoRight, const ImU32 &strokeColor, float rounding = 0.0f, int rounding_corners = ~0, float strokeThickness = 1.0f, bool antiAliased = true);
void ImDrawListAddEllipseWithHorizontalGradient(ImDrawList *dl, const ImVec2 &centre, const ImVec2 &radii, const ImU32 &fillColorLeft, const ImU32 &fillColorRight, const ImU32 &strokeColor, int num_segments = 12, float strokeThickness = 1.0f, bool antiAliased = true);
void ImDrawListAddCircleWithHorizontalGradient(ImDrawList *dl, const ImVec2 &centre, float radius, const ImU32 &fillColorLeft, const ImU32 &fillColorRight, const ImU32 &strokeColor, int num_segments = 12, float strokeThickness = 1.0f, bool antiAliased = true);
void ImDrawListAddRectWithHorizontalGradient(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, const ImU32 &fillColor, float fillColorGradientDeltaIn0_05, const ImU32 &strokeColor, float rounding = 0.0f, int rounding_corners = ~0, float strokeThickness = 1.0f, bool antiAliased = true);


#ifndef NO_IMGUIHELPER_VERTICAL_TEXT_METHODS
#   ifdef IMGUIHELPER_HAS_VERTICAL_TEXT_SUPPORT
#       warning Don't define IMGUIHELPER_HAS_VERTICAL_TEXT_SUPPORT yorself! It's a read-only definition!
#   else //IMGUIHELPER_HAS_VERTICAL_TEXT_SUPPORT
#       define IMGUIHELPER_HAS_VERTICAL_TEXT_SUPPORT
#   endif //IMGUIHELPER_HAS_VERTICAL_TEXT_SUPPORT
ImVec2 CalcVerticalTextSize(const char* text, const char* text_end = NULL, bool hide_text_after_double_hash = false, float wrap_width = -1.0f);
void RenderTextVertical(const ImFont* font,ImDrawList* draw_list, float size, ImVec2 pos, ImU32 col, const ImVec4& clip_rect, const char* text_begin, const char* text_end=NULL, float wrap_width=0.0f, bool cpu_fine_clip=false, bool rotateCCW=false);
void AddTextVertical(ImDrawList* drawList,const ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end=NULL, float wrap_width=0.0f, const ImVec4* cpu_fine_clip_rect=NULL,bool rotateCCW = false);
void AddTextVertical(ImDrawList* drawList,const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end=NULL,bool rotateCCW = false);
void RenderTextVerticalClipped(const ImVec2& pos_min, const ImVec2& pos_max, const char* text, const char* text_end, const ImVec2* text_size_if_known,const ImVec2& align =  ImVec2(0.0f,0.0f), const ImVec2* clip_min=NULL, const ImVec2* clip_max=NULL,bool rotateCCW=false);
#endif //NO_IMGUIHELPER_VERTICAL_TEXT_METHODS
#endif //NO_IMGUIHELPER_DRAW_METHODS

// These two methods are inspired by imguidock.cpp
// if optionalRootWindowName==NULL, they refer to the current window
// P.S. This methods are never used anywhere, and it's not clear to me when
// PutInForeground() is better then ImGui::SetWindowFocus()
void PutInBackground(const char* optionalRootWindowName=NULL);
void PutInForeground(const char* optionalRootWindowName=NULL);


#   ifdef IMGUI_USE_ZLIB	// requires linking to library -lZlib
// Two methods that fill rv and return true on success
#       ifndef NO_IMGUIHELPER_SERIALIZATION
#           ifndef NO_IMGUIHELPER_SERIALIZATION_LOAD
bool GzDecompressFromFile(const char* filePath,ImVector<char>& rv,bool clearRvBeforeUsage=true);
#   ifdef YES_IMGUISTRINGIFIER
bool GzBase64DecompressFromFile(const char* filePath,ImVector<char>& rv);
bool GzBase85DecompressFromFile(const char* filePath,ImVector<char>& rv);
#   endif //#YES_IMGUISTRINGIFIER
#           endif //NO_IMGUIHELPER_SERIALIZATION_LOAD
#       endif //NO_IMGUIHELPER_SERIALIZATION
bool GzDecompressFromMemory(const char* memoryBuffer,int memoryBufferSize,ImVector<char>& rv,bool clearRvBeforeUsage=true);
bool GzCompressFromMemory(const char* memoryBuffer,int memoryBufferSize,ImVector<char>& rv,bool clearRvBeforeUsage=true);
#   ifdef YES_IMGUISTRINGIFIER
bool GzBase64DecompressFromMemory(const char* input,ImVector<char>& rv);
bool GzBase85DecompressFromMemory(const char* input,ImVector<char>& rv);
bool GzBase64CompressFromMemory(const char* input,int inputSize,ImVector<char>& output,bool stringifiedMode=false,int numCharsPerLineInStringifiedMode=112);
bool GzBase85CompressFromMemory(const char* input,int inputSize,ImVector<char>& output,bool stringifiedMode=false,int numCharsPerLineInStringifiedMode=112);
#   endif //#YES_IMGUISTRINGIFIER
#   endif //IMGUI_USE_ZLIB

#   ifdef YES_IMGUIBZ2
// Two methods that fill rv and return true on success
#       ifndef NO_IMGUIHELPER_SERIALIZATION
#           ifndef NO_IMGUIHELPER_SERIALIZATION_LOAD
bool Bz2DecompressFromFile(const char* filePath,ImVector<char>& rv,bool clearRvBeforeUsage=true);
#   ifdef YES_IMGUISTRINGIFIER
bool Bz2Base64DecompressFromFile(const char* filePath, ImVector<char>& rv);
bool Bz2Base85DecompressFromFile(const char* filePath,ImVector<char>& rv);
#   endif //#YES_IMGUISTRINGIFIER
#           endif //NO_IMGUIHELPER_SERIALIZATION_LOAD
#       endif //NO_IMGUIHELPER_SERIALIZATION
// TODO: we can add helpers to compress bz2 as well...
#   endif //YES_IMGUIBZ2

#   ifdef YES_IMGUISTRINGIFIER
// Two methods that fill rv and return true on success
#       ifndef NO_IMGUIHELPER_SERIALIZATION
#           ifndef NO_IMGUIHELPER_SERIALIZATION_LOAD
bool Base64DecodeFromFile(const char* filePath,ImVector<char>& rv);
bool Base85DecodeFromFile(const char* filePath,ImVector<char>& rv);
#           endif //NO_IMGUIHELPER_SERIALIZATION_LOAD
#       endif //NO_IMGUIHELPER_SERIALIZATION
#   endif //YES_IMGUISTRINGIFIER

// IMPORTANT: FT_INT,FT_UNSIGNED,FT_FLOAT,FT_DOUBLE,FT_BOOL support from 1 to 4 components.
enum FieldType {
    FT_INT=0,
    FT_UNSIGNED,
    FT_FLOAT,
    FT_DOUBLE,
    //--------------- End types that support 1 to 4 array components ----------
    FT_STRING,      // an arbitrary-length string (or a char blob that can be used as custom type)
    FT_ENUM,        // serialized/deserialized as FT_INT
    FT_BOOL,
    FT_COLOR,       // serialized/deserialized as FT_FLOAT (with 3 or 4 components)
    FT_TEXTLINE,    // a (series of) text line(s) (separated by '\n') that are fed one at a time in the Deserializer callback
    FT_CUSTOM,      // a custom type that is served like FT_TEXTLINE (=one line at a time).
    FT_COUNT
};

}   // ImGui


// These classed are supposed to be used internally
namespace ImGuiHelper {
typedef ImGui::FieldType FieldType;

#ifndef NO_IMGUIHELPER_SERIALIZATION

#ifndef NO_IMGUIHELPER_SERIALIZATION_LOAD
bool GetFileContent(const char* filePath,ImVector<char>& contentOut,bool clearContentOutBeforeUsage=true,const char* modes="rb",bool appendTrailingZeroIfModesIsNotBinary=true);
bool FileExists(const char* filePath);

class Deserializer {
    char* f_data;
    size_t f_size;
    void clear();
    bool loadFromFile(const char* filename);
    bool allocate(size_t sizeToAllocate,const char* optionalTextToCopy=NULL,size_t optionalTextToCopySize=0);
    public:
    Deserializer() : f_data(NULL),f_size(0) {}
    Deserializer(const char* filename);                     // From file
    Deserializer(const char* text,size_t textSizeInBytes);  // From memory (and optionally from file through GetFileContent(...))
    ~Deserializer() {clear();}
    bool isValid() const {return (f_data && f_size>0);}

    // returns whether to stop parsing or not
    typedef bool (*ParseCallback)(FieldType ft,int numArrayElements,void* pValue,const char* name,void* userPtr);   // (*)
    // returns a pointer to "next_line" if the callback has stopped parsing or NULL.
    // returned value can be refeed as optionalBufferStart
    const char *parse(ParseCallback cb,void* userPtr,const char* optionalBufferStart=NULL) const;

    // (*)
    /*
    FT_CUSTOM and FT_TEXTLINE are served multiple times (one per text line) with numArrayElements that goes from 0 to numTextLines-1.
    All the other field types are served once.
    */

protected:
    void operator=(const Deserializer&) {}
    Deserializer(const Deserializer&) {}
};
#endif //NO_IMGUIHELPER_SERIALIZATION_LOAD

#ifndef NO_IMGUIHELPER_SERIALIZATION_SAVE
bool SetFileContent(const char *filePath, const unsigned char* content, int contentSize,const char* modes="wb");

class ISerializable;
class Serializer {

    ISerializable* f;
    void clear();

    public:
    Serializer(const char* filename);               // To file
    Serializer(int memoryBufferCapacity=2048);      // To memory (and optionally to file through WriteBufferToFile(...))
    ~Serializer();
    bool isValid() const {return (f);}

    bool save(FieldType ft, const float* pValue, const char* name, int numArrayElements=1,int prec=3);
    bool save(FieldType ft, const int* pValue, const char* name, int numArrayElements=1,int prec=-1);
    bool save(const float* pValue,const char* name,int numArrayElements=1,int prec=3)    {
        return save(ImGui::FT_FLOAT,pValue,name,numArrayElements,prec);
    }
    bool save(const int* pValue,const char* name,int numArrayElements=1,int prec=-1)  {
        return save(ImGui::FT_INT,pValue,name,numArrayElements,prec);
    }
    bool save(const char* pValue,const char* name,int pValueSize=-1);
    bool save(const unsigned* pValue, const char* name, int numArrayElements=1,int prec=-1);
    bool save(const double* pValue, const char* name, int numArrayElements=1,int prec=-1);
    bool save(const bool* pValue, const char* name, int numArrayElements=1);
    bool saveTextLines(const char* pValue,const char* name); // Splits the string into N lines: each line is passed by the deserializer into a single element in the callback
    bool saveTextLines(int numValues,bool (*items_getter)(void* data, int idx, const char** out_text),void* data,const char* name);

    // To serialize FT_CUSTOM:
    bool saveCustomFieldTypeHeader(const char* name, int numTextLines=1); //e.g. for 4 lines "[CUSTOM-4:MyCustomFieldTypeName]\n". Then add 4 lines using getPointer() below.

    // These 2 are only available when this class is constructed with the
    // Serializer(int memoryBufferCapacity) constructor
    const char* getBuffer() const;
    int getBufferSize() const;
    static bool WriteBufferToFile(const char* filename, const char* buffer, int bufferSize);

protected:
    void operator=(const Serializer&) {}
    Serializer(const Serializer&) {}

};
#endif //NO_IMGUIHELPER_SERIALIZATION_SAVE
#endif //NO_IMGUIHELPER_SERIALIZATION

// Optional String Helper methods:
// "destText" must be released with ImGui::MemFree(destText). It should always work.
void StringSet(char*& destText,const char* text,bool allowNullDestText=true);
// "destText" must be released with ImGui::MemFree(destText). It should always work.
void StringAppend(char*& destText, const char* textToAppend, bool allowNullDestText=true, bool prependLineFeedIfDestTextIsNotEmpty = true, bool mustAppendLineFeed = false);
// Appends a formatted string to a char vector (= no need to free memory)
// v can't be empty (it must at least be: v.size()==1 && v[0]=='\0')
// returns the number of chars appended.
int StringAppend(ImVector<char>& v,const char* fmt, ...);

} // ImGuiHelper

#ifndef NO_IMGUIKNOWNCOLOR_DEFINITIONS
#define KNOWNIMGUICOLOR_ALICEBLUE IM_COL32(240,248,255,255)
#define KNOWNIMGUICOLOR_ANTIQUEWHITE IM_COL32(250,235,215,255)
#define KNOWNIMGUICOLOR_AQUA IM_COL32(0,255,255,255)
#define KNOWNIMGUICOLOR_AQUAMARINE IM_COL32(127,255,212,255)
#define KNOWNIMGUICOLOR_AZURE IM_COL32(240,255,255,255)
#define KNOWNIMGUICOLOR_BEIGE IM_COL32(245,245,220,255)
#define KNOWNIMGUICOLOR_BISQUE IM_COL32(255,228,196,255)
#define KNOWNIMGUICOLOR_BLACK IM_COL32(0,0,0,255)
#define KNOWNIMGUICOLOR_BLANCHEDALMOND IM_COL32(255,235,205,255)
#define KNOWNIMGUICOLOR_BLUE IM_COL32(0,0,255,255)
#define KNOWNIMGUICOLOR_BLUEVIOLET IM_COL32(138,43,226,255)
#define KNOWNIMGUICOLOR_BROWN IM_COL32(165,42,42,255)
#define KNOWNIMGUICOLOR_BURLYWOOD IM_COL32(222,184,135,255)
#define KNOWNIMGUICOLOR_CADETBLUE IM_COL32(95,158,160,255)
#define KNOWNIMGUICOLOR_CHARTREUSE IM_COL32(127,255,0,255)
#define KNOWNIMGUICOLOR_CHOCOLATE IM_COL32(210,105,30,255)
#define KNOWNIMGUICOLOR_CORAL IM_COL32(255,127,80,255)
#define KNOWNIMGUICOLOR_CORNFLOWERBLUE IM_COL32(100,149,237,255)
#define KNOWNIMGUICOLOR_CORNSILK IM_COL32(255,248,220,255)
#define KNOWNIMGUICOLOR_CRIMSON IM_COL32(220,20,60,255)
#define KNOWNIMGUICOLOR_CYAN IM_COL32(0,255,255,255)
#define KNOWNIMGUICOLOR_DARKBLUE IM_COL32(0,0,139,255)
#define KNOWNIMGUICOLOR_DARKCYAN IM_COL32(0,139,139,255)
#define KNOWNIMGUICOLOR_DARKGOLDENROD IM_COL32(184,134,11,255)
#define KNOWNIMGUICOLOR_DARKGRAY IM_COL32(169,169,169,255)
#define KNOWNIMGUICOLOR_DARKGREEN IM_COL32(0,100,0,255)
#define KNOWNIMGUICOLOR_DARKKHAKI IM_COL32(189,183,107,255)
#define KNOWNIMGUICOLOR_DARKMAGENTA IM_COL32(139,0,139,255)
#define KNOWNIMGUICOLOR_DARKOLIVEGREEN IM_COL32(85,107,47,255)
#define KNOWNIMGUICOLOR_DARKORANGE IM_COL32(255,140,0,255)
#define KNOWNIMGUICOLOR_DARKORCHID IM_COL32(153,50,204,255)
#define KNOWNIMGUICOLOR_DARKRED IM_COL32(139,0,0,255)
#define KNOWNIMGUICOLOR_DARKSALMON IM_COL32(233,150,122,255)
#define KNOWNIMGUICOLOR_DARKSEAGREEN IM_COL32(143,188,139,255)
#define KNOWNIMGUICOLOR_DARKSLATEBLUE IM_COL32(72,61,139,255)
#define KNOWNIMGUICOLOR_DARKSLATEGRAY IM_COL32(47,79,79,255)
#define KNOWNIMGUICOLOR_DARKTURQUOISE IM_COL32(0,206,209,255)
#define KNOWNIMGUICOLOR_DARKVIOLET IM_COL32(148,0,211,255)
#define KNOWNIMGUICOLOR_DEEPPINK IM_COL32(255,20,147,255)
#define KNOWNIMGUICOLOR_DEEPSKYBLUE IM_COL32(0,191,255,255)
#define KNOWNIMGUICOLOR_DIMGRAY IM_COL32(105,105,105,255)
#define KNOWNIMGUICOLOR_DODGERBLUE IM_COL32(30,144,255,255)
#define KNOWNIMGUICOLOR_FIREBRICK IM_COL32(178,34,34,255)
#define KNOWNIMGUICOLOR_FLORALWHITE IM_COL32(255,250,240,255)
#define KNOWNIMGUICOLOR_FORESTGREEN IM_COL32(34,139,34,255)
#define KNOWNIMGUICOLOR_FUCHSIA IM_COL32(255,0,255,255)
#define KNOWNIMGUICOLOR_GAINSBORO IM_COL32(220,220,220,255)
#define KNOWNIMGUICOLOR_GHOSTWHITE IM_COL32(248,248,255,255)
#define KNOWNIMGUICOLOR_GOLD IM_COL32(255,215,0,255)
#define KNOWNIMGUICOLOR_GOLDENROD IM_COL32(218,165,32,255)
#define KNOWNIMGUICOLOR_GRAY IM_COL32(128,128,128,255)
#define KNOWNIMGUICOLOR_GREEN IM_COL32(0,128,0,255)
#define KNOWNIMGUICOLOR_GREENYELLOW IM_COL32(173,255,47,255)
#define KNOWNIMGUICOLOR_HONEYDEW IM_COL32(240,255,240,255)
#define KNOWNIMGUICOLOR_HOTPINK IM_COL32(255,105,180,255)
#define KNOWNIMGUICOLOR_INDIANRED IM_COL32(205,92,92,255)
#define KNOWNIMGUICOLOR_INDIGO IM_COL32(75,0,130,255)
#define KNOWNIMGUICOLOR_IVORY IM_COL32(255,255,240,255)
#define KNOWNIMGUICOLOR_KHAKI IM_COL32(240,230,140,255)
#define KNOWNIMGUICOLOR_LAVENDER IM_COL32(230,230,250,255)
#define KNOWNIMGUICOLOR_LAVENDERBLUSH IM_COL32(255,240,245,255)
#define KNOWNIMGUICOLOR_LAWNGREEN IM_COL32(124,252,0,255)
#define KNOWNIMGUICOLOR_LEMONCHIFFON IM_COL32(255,250,205,255)
#define KNOWNIMGUICOLOR_LIGHTBLUE IM_COL32(173,216,230,255)
#define KNOWNIMGUICOLOR_LIGHTCORAL IM_COL32(240,128,128,255)
#define KNOWNIMGUICOLOR_LIGHTCYAN IM_COL32(224,255,255,255)
#define KNOWNIMGUICOLOR_LIGHTGOLDENRODYELLOW IM_COL32(250,250,210,255)
#define KNOWNIMGUICOLOR_LIGHTGRAY IM_COL32(211,211,211,255)
#define KNOWNIMGUICOLOR_LIGHTGREEN IM_COL32(144,238,144,255)
#define KNOWNIMGUICOLOR_LIGHTPINK IM_COL32(255,182,193,255)
#define KNOWNIMGUICOLOR_LIGHTSALMON IM_COL32(255,160,122,255)
#define KNOWNIMGUICOLOR_LIGHTSEAGREEN IM_COL32(32,178,170,255)
#define KNOWNIMGUICOLOR_LIGHTSKYBLUE IM_COL32(135,206,250,255)
#define KNOWNIMGUICOLOR_LIGHTSLATEGRAY IM_COL32(119,136,153,255)
#define KNOWNIMGUICOLOR_LIGHTSTEELBLUE IM_COL32(176,196,222,255)
#define KNOWNIMGUICOLOR_LIGHTYELLOW IM_COL32(255,255,224,255)
#define KNOWNIMGUICOLOR_LIME IM_COL32(0,255,0,255)
#define KNOWNIMGUICOLOR_LIMEGREEN IM_COL32(50,205,50,255)
#define KNOWNIMGUICOLOR_LINEN IM_COL32(250,240,230,255)
#define KNOWNIMGUICOLOR_MAGENTA IM_COL32(255,0,255,255)
#define KNOWNIMGUICOLOR_MAROON IM_COL32(128,0,0,255)
#define KNOWNIMGUICOLOR_MEDIUMAQUAMARINE IM_COL32(102,205,170,255)
#define KNOWNIMGUICOLOR_MEDIUMBLUE IM_COL32(0,0,205,255)
#define KNOWNIMGUICOLOR_MEDIUMORCHID IM_COL32(186,85,211,255)
#define KNOWNIMGUICOLOR_MEDIUMPURPLE IM_COL32(147,112,219,255)
#define KNOWNIMGUICOLOR_MEDIUMSEAGREEN IM_COL32(60,179,113,255)
#define KNOWNIMGUICOLOR_MEDIUMSLATEBLUE IM_COL32(123,104,238,255)
#define KNOWNIMGUICOLOR_MEDIUMSPRINGGREEN IM_COL32(0,250,154,255)
#define KNOWNIMGUICOLOR_MEDIUMTURQUOISE IM_COL32(72,209,204,255)
#define KNOWNIMGUICOLOR_MEDIUMVIOLETRED IM_COL32(199,21,133,255)
#define KNOWNIMGUICOLOR_MIDNIGHTBLUE IM_COL32(25,25,112,255)
#define KNOWNIMGUICOLOR_MINTCREAM IM_COL32(245,255,250,255)
#define KNOWNIMGUICOLOR_MISTYROSE IM_COL32(255,228,225,255)
#define KNOWNIMGUICOLOR_MOCCASIN IM_COL32(255,228,181,255)
#define KNOWNIMGUICOLOR_NAVAJOWHITE IM_COL32(255,222,173,255)
#define KNOWNIMGUICOLOR_NAVY IM_COL32(0,0,128,255)
#define KNOWNIMGUICOLOR_OLDLACE IM_COL32(253,245,230,255)
#define KNOWNIMGUICOLOR_OLIVE IM_COL32(128,128,0,255)
#define KNOWNIMGUICOLOR_OLIVEDRAB IM_COL32(107,142,35,255)
#define KNOWNIMGUICOLOR_ORANGE IM_COL32(255,165,0,255)
#define KNOWNIMGUICOLOR_ORANGERED IM_COL32(255,69,0,255)
#define KNOWNIMGUICOLOR_ORCHID IM_COL32(218,112,214,255)
#define KNOWNIMGUICOLOR_PALEGOLDENROD IM_COL32(238,232,170,255)
#define KNOWNIMGUICOLOR_PALEGREEN IM_COL32(152,251,152,255)
#define KNOWNIMGUICOLOR_PALETURQUOISE IM_COL32(175,238,238,255)
#define KNOWNIMGUICOLOR_PALEVIOLETRED IM_COL32(219,112,147,255)
#define KNOWNIMGUICOLOR_PAPAYAWHIP IM_COL32(255,239,213,255)
#define KNOWNIMGUICOLOR_PEACHPUFF IM_COL32(255,218,185,255)
#define KNOWNIMGUICOLOR_PERU IM_COL32(205,133,63,255)
#define KNOWNIMGUICOLOR_PINK IM_COL32(255,192,203,255)
#define KNOWNIMGUICOLOR_PLUM IM_COL32(221,160,221,255)
#define KNOWNIMGUICOLOR_POWDERBLUE IM_COL32(176,224,230,255)
#define KNOWNIMGUICOLOR_PURPLE IM_COL32(128,0,128,255)
#define KNOWNIMGUICOLOR_RED IM_COL32(255,0,0,255)
#define KNOWNIMGUICOLOR_ROSYBROWN IM_COL32(188,143,143,255)
#define KNOWNIMGUICOLOR_ROYALBLUE IM_COL32(65,105,225,255)
#define KNOWNIMGUICOLOR_SADDLEBROWN IM_COL32(139,69,19,255)
#define KNOWNIMGUICOLOR_SALMON IM_COL32(250,128,114,255)
#define KNOWNIMGUICOLOR_SANDYBROWN IM_COL32(244,164,96,255)
#define KNOWNIMGUICOLOR_SEAGREEN IM_COL32(46,139,87,255)
#define KNOWNIMGUICOLOR_SEASHELL IM_COL32(255,245,238,255)
#define KNOWNIMGUICOLOR_SIENNA IM_COL32(160,82,45,255)
#define KNOWNIMGUICOLOR_SILVER IM_COL32(192,192,192,255)
#define KNOWNIMGUICOLOR_SKYBLUE IM_COL32(135,206,235,255)
#define KNOWNIMGUICOLOR_SLATEBLUE IM_COL32(106,90,205,255)
#define KNOWNIMGUICOLOR_SLATEGRAY IM_COL32(112,128,144,255)
#define KNOWNIMGUICOLOR_SNOW IM_COL32(255,250,250,255)
#define KNOWNIMGUICOLOR_SPRINGGREEN IM_COL32(0,255,127,255)
#define KNOWNIMGUICOLOR_STEELBLUE IM_COL32(70,130,180,255)
#define KNOWNIMGUICOLOR_TAN IM_COL32(210,180,140,255)
#define KNOWNIMGUICOLOR_TEAL IM_COL32(0,128,128,255)
#define KNOWNIMGUICOLOR_THISTLE IM_COL32(216,191,216,255)
#define KNOWNIMGUICOLOR_TOMATO IM_COL32(255,99,71,255)
#define KNOWNIMGUICOLOR_TURQUOISE IM_COL32(64,224,208,255)
#define KNOWNIMGUICOLOR_VIOLET IM_COL32(238,130,238,255)
#define KNOWNIMGUICOLOR_WHEAT IM_COL32(245,222,179,255)
#define KNOWNIMGUICOLOR_WHITE IM_COL32(255,255,255,255)
#define KNOWNIMGUICOLOR_WHITESMOKE IM_COL32(245,245,245,255)
#define KNOWNIMGUICOLOR_YELLOW IM_COL32(255,255,0,255)
#define KNOWNIMGUICOLOR_YELLOWGREEN IM_COL32(154,205,50,255)
#endif // NO_IMGUIKNOWNCOLOR_DEFINITIONS

#endif //IMGUIHELPER_H_

