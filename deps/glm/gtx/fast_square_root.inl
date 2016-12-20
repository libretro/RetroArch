///////////////////////////////////////////////////////////////////////////////////////////////////
// OpenGL Mathematics Copyright (c) 2005 - 2014 G-Truc Creation (www.g-truc.net)
///////////////////////////////////////////////////////////////////////////////////////////////////
// Created : 2006-01-04
// Updated : 2011-10-14
// Licence : This source is under MIT License
// File    : glm/gtx/fast_square_root.inl
///////////////////////////////////////////////////////////////////////////////////////////////////

namespace glm
{
	// fastSqrt
	template <typename genType>
	inline genType fastSqrt
	(
		genType const & x
	)
	{
		return genType(1) / fastInverseSqrt(x);
	}

	VECTORIZE_VEC(fastSqrt)

	// fastInversesqrt
	template <>
	inline float fastInverseSqrt<float>(float const & x)
	{
      return detail::compute_inversesqrt<detail::tvec1, float, lowp>::call(detail::tvec1<float, lowp>(x)).x;
	}

	template <>
	inline double fastInverseSqrt<double>(double const & x)
	{
      return detail::compute_inversesqrt<detail::tvec1, double, lowp>::call(detail::tvec1<double, lowp>(x)).x;
	}

	template <template <class, precision> class vecType, typename T, precision P>
	inline vecType<T, P> fastInverseSqrt
	(
		vecType<T, P> const & x
	)
	{
		return detail::compute_inversesqrt<vecType, T, P>::call(x);
	}

	VECTORIZE_VEC(fastInverseSqrt)

	// fastLength
	template <typename genType>
	inline genType fastLength
	(
		genType const & x
	)
	{
		return abs(x);
	}

	template <typename valType, precision P>
	inline valType fastLength
	(
		detail::tvec2<valType, P> const & x
	)
	{
		valType sqr = x.x * x.x + x.y * x.y;
		return fastSqrt(sqr);
	}

	template <typename valType, precision P>
	inline valType fastLength
	(
		detail::tvec3<valType, P> const & x
	)
	{
		valType sqr = x.x * x.x + x.y * x.y + x.z * x.z;
		return fastSqrt(sqr);
	}

	template <typename valType, precision P>
	inline valType fastLength
	(
		detail::tvec4<valType, P> const & x
	)
	{
		valType sqr = x.x * x.x + x.y * x.y + x.z * x.z + x.w * x.w;
		return fastSqrt(sqr);
	}

	// fastDistance
	template <typename genType>
	inline genType fastDistance
	(
		genType const & x, 
		genType const & y
	)
	{
		return fastLength(y - x);
	}

	// fastNormalize
	template <typename genType>
	inline genType fastNormalize
	(
		genType const & x
	)
	{
		return x > genType(0) ? genType(1) : -genType(1);
	}

	template <typename valType, precision P>
	inline detail::tvec2<valType, P> fastNormalize
	(
		detail::tvec2<valType, P> const & x
	)
	{
		valType sqr = x.x * x.x + x.y * x.y;
		return x * fastInverseSqrt(sqr);
	}

	template <typename valType, precision P>
	inline detail::tvec3<valType, P> fastNormalize
	(
		detail::tvec3<valType, P> const & x
	)
	{
		valType sqr = x.x * x.x + x.y * x.y + x.z * x.z;
		return x * fastInverseSqrt(sqr);
	}

	template <typename valType, precision P>
	inline detail::tvec4<valType, P> fastNormalize
	(
		detail::tvec4<valType, P> const & x
	)
	{
		valType sqr = x.x * x.x + x.y * x.y + x.z * x.z + x.w * x.w;
		return x * fastInverseSqrt(sqr);
	}
}//namespace glm
