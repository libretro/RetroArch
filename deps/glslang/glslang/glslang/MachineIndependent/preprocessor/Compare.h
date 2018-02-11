#ifndef _MACHINE_INDEPENDENT_COMPARE_H
#define _MACHINE_INDEPENDENT_COMPARE_H

#include "../../../hlsl/hlslTokens.h"

namespace {

	struct str_eq
	{
		bool operator()(const char* lhs, const char* rhs) const
		{
			return strcmp(lhs, rhs) == 0;
		}
	};

	struct str_hash
	{
		size_t operator()(const char* str) const
		{
			// djb2
			unsigned long hash = 5381;
			int c;

			while ((c = *str++) != 0)
				hash = ((hash << 5) + hash) + c;

			return hash;
		}
	};
};

#endif
