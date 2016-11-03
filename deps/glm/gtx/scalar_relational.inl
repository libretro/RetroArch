///////////////////////////////////////////////////////////////////////////////////////////////////
// OpenGL Mathematics Copyright (c) 2005 - 2012 G-Truc Creation (www.g-truc.net)
///////////////////////////////////////////////////////////////////////////////////////////////////
// Created : 2013-02-04
// Updated : 2013-02-04
// Licence : This source is under MIT License
// File    : glm/gtx/scalar_relational.inl
///////////////////////////////////////////////////////////////////////////////////////////////////

namespace glm
{
	template <typename T>
	inline bool lessThan
	(
		T const & x, 
		T const & y
	)
	{
		return x < y;
	}

	template <typename T>
	inline bool lessThanEqual
	(
		T const & x, 
		T const & y
	)
	{
		return x <= y;
	}

	template <typename T>
	inline bool greaterThan
	(
		T const & x, 
		T const & y
	)
	{
		return x > y;
	}

	template <typename T>
	inline bool greaterThanEqual
	(
		T const & x, 
		T const & y
	)
	{
		return x >= y;
	}

	template <typename T>
	inline bool equal
	(
		T const & x, 
		T const & y
	)
	{
		return x == y;
	}

	template <typename T>
	inline bool notEqual
	(
		T const & x, 
		T const & y
	)
	{
		return x != y;
	}

	inline bool any
	(
		bool const & x
	)
	{
		return x;
	}

	inline bool all
	(
		bool const & x
	)
	{
		return x;
	}

	inline bool not_
	(
		bool const & x
	)
	{
		return !x;
	}
}//namespace glm
