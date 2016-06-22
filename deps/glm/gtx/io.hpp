///////////////////////////////////////////////////////////////////////////////////
/// OpenGL Mathematics (glm.g-truc.net)
///
/// Copyright (c) 2005 - 2014 G-Truc Creation (www.g-truc.net)
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
/// 
/// The above copyright notice and this permission notice shall be included in
/// all copies or substantial portions of the Software.
/// 
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
/// THE SOFTWARE.
///
/// @ref gtx_io
/// @file glm/gtx/io.hpp
/// @date 2013-11-22
/// @author Jan P Springer (regnirpsj@gmail.com)
///
/// @see core (dependence)
/// @see gtx_quaternion (dependence)
///
/// @defgroup gtx_io GLM_GTX_io
/// @ingroup gtx
/// 
/// @brief std::[w]ostream support for glm types
///
/// <glm/gtx/io.hpp> needs to be included to use these functionalities.
///////////////////////////////////////////////////////////////////////////////////

#ifndef GLM_GTX_io
#define GLM_GTX_io

// Dependency:
#include "../detail/setup.hpp"
#include "../gtc/quaternion.hpp"

#if(defined(GLM_MESSAGES) && !defined(GLM_EXT_INCLUDED))
#	pragma message("GLM: GLM_GTX_io extension included")
#endif

#include <iosfwd>  // std::basic_ostream<> (fwd)
#include <utility> // std::pair<>

namespace glm
{
	/// @addtogroup gtx_io
	/// @{
  
  namespace io
  {
    
    class precision_guard {

    public:
      
      GLM_FUNC_DECL explicit precision_guard();
      GLM_FUNC_DECL         ~precision_guard();
                
    private:

      unsigned precision_;
      unsigned value_width_;
      
    };

    class format_guard
	{
	public:
		enum order_t { column_major, row_major, };

		GLM_FUNC_DECL explicit format_guard();
		GLM_FUNC_DECL         ~format_guard();

	private:

		order_t order_;
		char    cr_;
	};

    // decimal places (dflt: 3)
    GLM_FUNC_DECL unsigned& precision();

    // sign + value + '.' + decimals (dflt: 1 + 4 + 1 + precision())
    GLM_FUNC_DECL unsigned& value_width();

    // matrix output order (dflt: row_major)
    GLM_FUNC_DECL format_guard::order_t& order();

    // carriage/return char (dflt: '\n')
    GLM_FUNC_DECL char& cr();

    // matrix output order -> column_major
    GLM_FUNC_DECL std::ios_base& column_major(std::ios_base&);

    // matrix output order -> row_major
    GLM_FUNC_DECL std::ios_base& row_major   (std::ios_base&);

    // carriage/return char -> '\n'
    GLM_FUNC_DECL std::ios_base& formatted   (std::ios_base&);

    // carriage/return char -> ' '
    GLM_FUNC_DECL std::ios_base& unformatted (std::ios_base&);

  }//namespace io

  namespace detail
  {
    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tquat<T,P> const&);
    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tvec2<T,P> const&);
    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tvec3<T,P> const&);
    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tvec4<T,P> const&);
    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tmat2x2<T,P> const&);
    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tmat2x3<T,P> const&);
    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tmat2x4<T,P> const&);
    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tmat3x2<T,P> const&);
    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tmat3x3<T,P> const&);
    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tmat3x4<T,P> const&);
    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tmat4x2<T,P> const&);
    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tmat4x3<T,P> const&);
    template <typename CTy, typename CTr, typename T, precision P>
    GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tmat4x4<T,P> const&);

	/// @}  
}//namespace detail
}//namespace glm

#include "io.inl"

#endif//GLM_GTX_io
