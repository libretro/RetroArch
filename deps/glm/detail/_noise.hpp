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
/// @ref core
/// @file glm/detail/_noise.hpp
/// @date 2013-12-24 / 2013-12-24
/// @author Christophe Riccio
///////////////////////////////////////////////////////////////////////////////////

#ifndef GLM_DETAIL_NOISE_INCLUDED
#define GLM_DETAIL_NOISE_INCLUDED

namespace glm{
namespace detail
{
	template <typename T>
	inline T mod289(T const & x)
	{
		return x - floor(x * static_cast<T>(1.0) / static_cast<T>(289.0)) * static_cast<T>(289.0);
	}

	template <typename T>
	inline T permute(T const & x)
	{
		return mod289(((x * static_cast<T>(34)) + static_cast<T>(1)) * x);
	}

	template <typename T, precision P>
	inline tvec2<T, P> permute(tvec2<T, P> const & x)
	{
		return mod289(((x * static_cast<T>(34)) + static_cast<T>(1)) * x);
	}
	
	template <typename T, precision P>
	inline tvec3<T, P> permute(tvec3<T, P> const & x)
	{
		return mod289(((x * static_cast<T>(34)) + static_cast<T>(1)) * x);
	}
	
	template <typename T, precision P>
	inline tvec4<T, P> permute(tvec4<T, P> const & x)
	{
		return mod289(((x * static_cast<T>(34)) + static_cast<T>(1)) * x);
	}
	template <typename T>
	inline T taylorInvSqrt(T const & r)
	{
		return T(1.79284291400159) - T(0.85373472095314) * r;
	}
	
	template <typename T, precision P>
	inline detail::tvec2<T, P> taylorInvSqrt(detail::tvec2<T, P> const & r)
	{
		return T(1.79284291400159) - T(0.85373472095314) * r;
	}
	
	template <typename T, precision P>
	inline detail::tvec3<T, P> taylorInvSqrt(detail::tvec3<T, P> const & r)
	{
		return T(1.79284291400159) - T(0.85373472095314) * r;
	}
	
	template <typename T, precision P>
	inline detail::tvec4<T, P> taylorInvSqrt(detail::tvec4<T, P> const & r)
	{
		return T(1.79284291400159) - T(0.85373472095314) * r;
	}
	
	template <typename T, precision P>
	inline detail::tvec2<T, P> fade(detail::tvec2<T, P> const & t)
	{
		return (t * t * t) * (t * (t * T(6) - T(15)) + T(10));
	}
	
	template <typename T, precision P>
	inline detail::tvec3<T, P> fade(detail::tvec3<T, P> const & t)
	{
		return (t * t * t) * (t * (t * T(6) - T(15)) + T(10));
	}
	
	template <typename T, precision P>
	inline detail::tvec4<T, P> fade(detail::tvec4<T, P> const & t)
	{
		return (t * t * t) * (t * (t * T(6) - T(15)) + T(10));
	}
}//namespace detail
}//namespace glm

#endif//GLM_DETAIL_NOISE_INCLUDED

