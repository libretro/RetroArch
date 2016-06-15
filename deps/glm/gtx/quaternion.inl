///////////////////////////////////////////////////////////////////////////////////////////////////
// OpenGL Mathematics Copyright (c) 2005 - 2014 G-Truc Creation (www.g-truc.net)
///////////////////////////////////////////////////////////////////////////////////////////////////
// Created : 2005-12-21
// Updated : 2008-11-27
// Licence : This source is under MIT License
// File    : glm/gtx/quaternion.inl
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <limits>

namespace glm
{
	template <typename T, precision P>
	GLM_FUNC_QUALIFIER detail::tvec3<T, P> cross
	(
		detail::tvec3<T, P> const & v,
		detail::tquat<T, P> const & q
	)
	{
		return inverse(q) * v;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER detail::tvec3<T, P> cross
	(
		detail::tquat<T, P> const & q,
		detail::tvec3<T, P> const & v
	)
	{
		return q * v;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER detail::tquat<T, P> squad
	(
		detail::tquat<T, P> const & q1,
		detail::tquat<T, P> const & q2,
		detail::tquat<T, P> const & s1,
		detail::tquat<T, P> const & s2,
		T const & h)
	{
		return mix(mix(q1, q2, h), mix(s1, s2, h), T(2) * (T(1) - h) * h);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER detail::tquat<T, P> intermediate
	(
		detail::tquat<T, P> const & prev,
		detail::tquat<T, P> const & curr,
		detail::tquat<T, P> const & next
	)
	{
		detail::tquat<T, P> invQuat = inverse(curr);
		return exp((log(next + invQuat) + log(prev + invQuat)) / T(-4)) * curr;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER detail::tquat<T, P> exp
	(
		detail::tquat<T, P> const & q
	)
	{
		detail::tvec3<T, P> u(q.x, q.y, q.z);
		float Angle = glm::length(u);
		detail::tvec3<T, P> v(u / Angle);
		return detail::tquat<T, P>(cos(Angle), sin(Angle) * v);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER detail::tquat<T, P> log
	(
		detail::tquat<T, P> const & q
	)
	{
		if((q.x == static_cast<T>(0)) && (q.y == static_cast<T>(0)) && (q.z == static_cast<T>(0)))
		{
			if(q.w > T(0))
				return detail::tquat<T, P>(log(q.w), T(0), T(0), T(0));
			else if(q.w < T(0))
				return detail::tquat<T, P>(log(-q.w), T(3.1415926535897932384626433832795), T(0),T(0));
			else
				return detail::tquat<T, P>(std::numeric_limits<T>::infinity(), std::numeric_limits<T>::infinity(), std::numeric_limits<T>::infinity(), std::numeric_limits<T>::infinity());
		}
		else
		{
			T Vec3Len = sqrt(q.x * q.x + q.y * q.y + q.z * q.z);
			T QuatLen = sqrt(Vec3Len * Vec3Len + q.w * q.w);
			T t = atan(Vec3Len, T(q.w)) / Vec3Len;
			return detail::tquat<T, P>(t * q.x, t * q.y, t * q.z, log(QuatLen));
		}
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER detail::tquat<T, P> pow
	(
		detail::tquat<T, P> const & x,
		T const & y
	)
	{
		if(abs(x.w) > T(0.9999))
			return x;
		float Angle = acos(y);
		float NewAngle = Angle * y;
		float Div = sin(NewAngle) / sin(Angle);
		return detail::tquat<T, P>(
			cos(NewAngle),
			x.x * Div,
			x.y * Div,
			x.z * Div);
	}

	//template <typename T, precision P>
	//GLM_FUNC_QUALIFIER detail::tquat<T, P> sqrt
	//(
	//	detail::tquat<T, P> const & q
	//)
	//{
	//	T q0 = static_cast<T>(1) - dot(q, q);
	//	return T(2) * (T(1) + q0) * q;
	//}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER detail::tvec3<T, P> rotate
	(
		detail::tquat<T, P> const & q,
		detail::tvec3<T, P> const & v
	)
	{
		return q * v;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER detail::tvec4<T, P> rotate
	(
		detail::tquat<T, P> const & q,
		detail::tvec4<T, P> const & v
	)
	{
		return q * v;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER T extractRealComponent
	(
		detail::tquat<T, P> const & q
	)
	{
		T w = static_cast<T>(1.0) - q.x * q.x - q.y * q.y - q.z * q.z;
		if(w < T(0))
			return T(0);
		else
			return -sqrt(w);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER T length2
	(
		detail::tquat<T, P> const & q
	)
	{
		return q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER detail::tquat<T, P> shortMix
	(
		detail::tquat<T, P> const & x,
		detail::tquat<T, P> const & y,
		T const & a
	)
	{
		if(a <= T(0)) return x;
		if(a >= T(1)) return y;

		T fCos = dot(x, y);
		detail::tquat<T, P> y2(y); //BUG!!! tquat<T> y2;
		if(fCos < T(0))
		{
			y2 = -y;
			fCos = -fCos;
		}

		//if(fCos > 1.0f) // problem
		T k0, k1;
		if(fCos > T(0.9999))
		{
			k0 = static_cast<T>(1) - a;
			k1 = static_cast<T>(0) + a; //BUG!!! 1.0f + a;
		}
		else
		{
			T fSin = sqrt(T(1) - fCos * fCos);
			T fAngle = atan(fSin, fCos);
			T fOneOverSin = static_cast<T>(1) / fSin;
			k0 = sin((T(1) - a) * fAngle) * fOneOverSin;
			k1 = sin((T(0) + a) * fAngle) * fOneOverSin;
		}

		return detail::tquat<T, P>(
			k0 * x.w + k1 * y2.w,
			k0 * x.x + k1 * y2.x,
			k0 * x.y + k1 * y2.y,
			k0 * x.z + k1 * y2.z);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER detail::tquat<T, P> fastMix
	(
		detail::tquat<T, P> const & x,
		detail::tquat<T, P> const & y,
		T const & a
	)
	{
		return glm::normalize(x * (T(1) - a) + (y * a));
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER detail::tquat<T, P> rotation
	(
		detail::tvec3<T, P> const & orig,
		detail::tvec3<T, P> const & dest
	)
	{
		T cosTheta = dot(orig, dest);
		detail::tvec3<T, P> rotationAxis;

		if(cosTheta < T(-1) + epsilon<T>())
		{
			// special case when vectors in opposite directions :
			// there is no "ideal" rotation axis
			// So guess one; any will do as long as it's perpendicular to start
			// This implementation favors a rotation around the Up axis (Y),
			// since it's often what you want to do.
			rotationAxis = cross(detail::tvec3<T, P>(0, 0, 1), orig);
			if(length2(rotationAxis) < epsilon<T>()) // bad luck, they were parallel, try again!
				rotationAxis = cross(detail::tvec3<T, P>(1, 0, 0), orig);

			rotationAxis = normalize(rotationAxis);
			return angleAxis(pi<T>(), rotationAxis);
		}

		// Implementation from Stan Melax's Game Programming Gems 1 article
		rotationAxis = cross(orig, dest);

		T s = sqrt((T(1) + cosTheta) * T(2));
		T invs = static_cast<T>(1) / s;

		return detail::tquat<T, P>(
			s * T(0.5f), 
			rotationAxis.x * invs,
			rotationAxis.y * invs,
			rotationAxis.z * invs);
	}

}//namespace glm
