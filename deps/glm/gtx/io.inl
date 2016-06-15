///////////////////////////////////////////////////////////////////////////////////////////////////
// OpenGL Mathematics Copyright (c) 2005 - 2014 G-Truc Creation (www.g-truc.net)
///////////////////////////////////////////////////////////////////////////////////////////////////
// Created : 2013-11-22
// Updated : 2013-11-22
// Licence : This source is under MIT License
// File    : glm/gtx/inl.inl
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "../matrix.hpp"
// #include <boost/io/ios_state.hpp> // boost::io::ios_all_saver
#include <iomanip>                // std::setfill<>, std::fixed, std::setprecision, std::right,
                                  // std::setw
#include <ostream>                // std::basic_ostream<>

namespace glm{
namespace io
{
  
    /* explicit */ GLM_FUNC_QUALIFIER
    precision_guard::precision_guard()
      : precision_  (precision()),
        value_width_(value_width())
    {}

    GLM_FUNC_QUALIFIER
    precision_guard::~precision_guard()
    {
      value_width() = value_width_;
      precision()   = precision_;
    }

    /* explicit */ GLM_FUNC_QUALIFIER
    format_guard::format_guard()
      : order_(order()),
        cr_   (cr())
    {}

    GLM_FUNC_QUALIFIER
    format_guard::~format_guard()
    {
      cr()    = cr_;
      order() = order_;
    }

    GLM_FUNC_QUALIFIER unsigned& precision()
    {
      static unsigned p(3);

      return p;
    }
    
    GLM_FUNC_QUALIFIER unsigned& value_width()
    {
      static unsigned p(9);

      return p;
    }
    
    GLM_FUNC_QUALIFIER format_guard::order_t& order()
    {
      static format_guard::order_t p(format_guard::row_major);

      return p;
    }
    
    GLM_FUNC_QUALIFIER char&
    cr()
    {
      static char p('\n'); return p;
    }
    
    GLM_FUNC_QUALIFIER std::ios_base& column_major(std::ios_base& os)
    {
      order() = format_guard::column_major;
      
      return os;
    }
    
    GLM_FUNC_QUALIFIER std::ios_base& row_major(std::ios_base& os)
    {
      order() = format_guard::row_major;
      
      return os;
    }

    GLM_FUNC_QUALIFIER std::ios_base& formatted(std::ios_base& os)
    {
      cr() = '\n';
      
      return os;
    }
    
    GLM_FUNC_QUALIFIER std::ios_base& unformatted(std::ios_base& os)
    {
      cr() = ' ';
      
      return os;
    }
    
} // namespace io
namespace detail
{
    // functions, inlined (inline)

    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_QUALIFIER std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>& os, tquat<T,P> const& a)
    {
      typename std::basic_ostream<CTy,CTr>::sentry const cerberus(os);

      if (cerberus) {
        // boost::io::ios_all_saver const ias(os);
      
        os << std::fixed << std::setprecision(io::precision())
           << '['
           << std::right << std::setfill<CTy>(' ') << std::setw(io::value_width()) << a.w << ','
           << std::right << std::setfill<CTy>(' ') << std::setw(io::value_width()) << a.x << ','
           << std::right << std::setfill<CTy>(' ') << std::setw(io::value_width()) << a.y << ','
           << std::right << std::setfill<CTy>(' ') << std::setw(io::value_width()) << a.z
           << ']';
      }

      return os;
    }
    
    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_QUALIFIER std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>& os, tvec2<T,P> const& a)
    {
      typename std::basic_ostream<CTy,CTr>::sentry const cerberus(os);

      if (cerberus) {
        // boost::io::ios_all_saver const ias(os);
      
        os << std::fixed << std::setprecision(io::precision())
           << '['
           << std::right << std::setfill<CTy>(' ') << std::setw(io::value_width()) << a.x << ','
           << std::right << std::setfill<CTy>(' ') << std::setw(io::value_width()) << a.y
           << ']';
      }

      return os;
    }
  
    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_QUALIFIER std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>& os, tvec3<T,P> const& a)
    {
      typename std::basic_ostream<CTy,CTr>::sentry const cerberus(os);

      if (cerberus) {
        // boost::io::ios_all_saver const ias(os);
      
        os << std::fixed << std::setprecision(io::precision())
           << '['
           << std::right << std::setfill<CTy>(' ') << std::setw(io::value_width()) << a.x << ','
           << std::right << std::setfill<CTy>(' ') << std::setw(io::value_width()) << a.y << ','
           << std::right << std::setfill<CTy>(' ') << std::setw(io::value_width()) << a.z
           << ']';
      }

      return os;
    }

    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_QUALIFIER std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>& os, tvec4<T,P> const& a)
    {
      typename std::basic_ostream<CTy,CTr>::sentry const cerberus(os);

      if (cerberus) {
        // boost::io::ios_all_saver const ias(os);
      
        os << std::fixed << std::setprecision(io::precision())
           << '['
           << std::right << std::setfill<CTy>(' ') << std::setw(io::value_width()) << a.x << ','
           << std::right << std::setfill<CTy>(' ') << std::setw(io::value_width()) << a.y << ','
           << std::right << std::setfill<CTy>(' ') << std::setw(io::value_width()) << a.z << ','
           << std::right << std::setfill<CTy>(' ') << std::setw(io::value_width()) << a.w
           << ']';
      }

      return os;
    }

    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_QUALIFIER std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>& os, tmat2x2<T,P> const& m)
    {
      typename std::basic_ostream<CTy,CTr>::sentry const cerberus(os);

      if (cerberus) {
        
        os << io::cr()
           << '[' << m[0] << io::cr()
           << ' ' << m[1] << ']';
      }

      return os;
    }

    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_QUALIFIER std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>& os, tmat2x3<T,P> const& m)
    {
      typename std::basic_ostream<CTy,CTr>::sentry const cerberus(os);

      if (cerberus) {

        os << io::cr()
           << '[' << m[0] << io::cr()
           << ' ' << m[1] << ']';
      }

      return os;
    }

    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_QUALIFIER std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>& os, tmat2x4<T,P> const& m)
    {
      typename std::basic_ostream<CTy,CTr>::sentry const cerberus(os);

      if (cerberus) {

        os << io::cr()
           << '[' << m[0] << io::cr()
           << ' ' << m[1] << ']';
      }

      return os;
    }

    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_QUALIFIER std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>& os, tmat3x2<T,P> const& m)
    {
      typename std::basic_ostream<CTy,CTr>::sentry const cerberus(os);

      if (cerberus) {

        os << io::cr()
           << '[' << m[0] << io::cr()
           << ' ' << m[1] << io::cr()
           << ' ' << m[2] << ']';
      }

      return os;
    }

    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_QUALIFIER std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>& os, tmat3x3<T,P> const& m)
    {
      typename std::basic_ostream<CTy,CTr>::sentry const cerberus(os);

      if (cerberus) {

        os << io::cr()
           << '[' << m[0] << io::cr()
           << ' ' << m[1] << io::cr()
           << ' ' << m[2] << ']';
      }

      return os;
    }

    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_QUALIFIER std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>& os, tmat3x4<T,P> const& m)
    {
      typename std::basic_ostream<CTy,CTr>::sentry const cerberus(os);

      if (cerberus) {

        os << io::cr()
           << '[' << m[0] << io::cr()
           << ' ' << m[1] << io::cr()
           << ' ' << m[2] << ']';
      }

      return os;
    }

    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_QUALIFIER std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>& os, tmat4x2<T,P> const& m)
    {
      typename std::basic_ostream<CTy,CTr>::sentry const cerberus(os);

      if (cerberus) {

        os << io::cr()
           << '[' << m[0] << io::cr()
           << ' ' << m[1] << io::cr()
           << ' ' << m[2] << io::cr()
           << ' ' << m[3] << ']';
      }

      return os;
    }

    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_QUALIFIER std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>& os, tmat4x3<T,P> const& m)
    {
      typename std::basic_ostream<CTy,CTr>::sentry const cerberus(os);

      if (cerberus) {

        os << io::cr()
           << '[' << m[0] << io::cr()
           << ' ' << m[1] << io::cr()
           << ' ' << m[2] << io::cr()
           << ' ' << m[3] << ']';
      }

      return os;
    }

    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_QUALIFIER std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>& os, tmat4x4<T,P> const& m)
    {
      typename std::basic_ostream<CTy,CTr>::sentry const cerberus(os);

      if (cerberus) {

        os << io::cr()
           << '[' << m[0] << io::cr()
           << ' ' << m[1] << io::cr()
           << ' ' << m[2] << io::cr()
           << ' ' << m[3] << ']';
      }

      return os;
    }

}//namespace detail
}//namespace glm
