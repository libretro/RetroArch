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

#include <imgui.h>
#include <stddef.h>
#include <stdarg.h>

// Must be at most 65535
#ifndef IMGUIAL_LOG_MAX_LINE_SIZE
#define IMGUIAL_LOG_MAX_LINE_SIZE 1024
#endif

// Must be at least MAX_LINE_SIZE + 3
#ifndef IMGUIAL_LOG_MAX_BUFFER_SIZE
#define IMGUIAL_LOG_MAX_BUFFER_SIZE 65536
#endif

namespace ImGuiAl
{
  class Log
  {
  public:
    enum Level
    {
      kDebug = 0,
      kInfo  = 1,
      kWarn  = 2,
      kError = 3
    };
    
    enum Flags
    {
      kShowFilters = 1 << 0
    };
    
    typedef bool ( *IterateFunc )( Level level, const char* line, void *ud );
    
    inline Log()
    {
      Init();
    }
    
    virtual ~Log();
    
    bool Init( unsigned flags = 0, const char** more_actions = NULL );
    
    void SetColor( Level level, float r, float g, float b );
    void SetLabel( Level level, const char* label );
    void SetCumulativeLabel( const char* label );
    void SetFilterHeaderLabel( const char* label );
    void SetFilterLabel( const char* label );
    
    void VPrintf( Level level, const char* format, va_list args );
    
    void Debug( const char* format, ... );
    void Info( const char* format, ... );
    void Warn( const char* format, ... );
    void Error( const char* format, ... );
    
    inline void Clear()
    {
      m_First = m_Last = 0;
      m_Avail = IMGUIAL_LOG_MAX_BUFFER_SIZE;
      m_ScrollToBottom = true;
    }
    
    void Iterate( IterateFunc iterator, bool apply_filters, void* ud );
    
    int Draw();
    
  protected:
    void   Write( const void* data, size_t size );
    void   Read( void* data, size_t size );
    size_t Peek( size_t pos, void* data, size_t size );
    
    inline void Skip( size_t size )
    {
      m_First = ( m_First + size ) % IMGUIAL_LOG_MAX_BUFFER_SIZE;
      m_Avail += size;
    }
    
    char            m_Buffer[ IMGUIAL_LOG_MAX_BUFFER_SIZE ];
    size_t          m_Avail;
    size_t          m_First;
    size_t          m_Last;
    unsigned        m_Flags;
    const char**    m_MoreActions;
    Level           m_Level;
    bool            m_Cumulative;
    ImGuiTextFilter m_Filter;
    bool            m_ScrollToBottom;
    ImColor         m_Colors[ 4 ][ 4 ];
    const char*     m_Labels[ 4 ];
    const char*     m_CumulativeLabel;
    const char*     m_FilterHeaderLabel;
    const char*     m_FilterLabel;
  };
}
