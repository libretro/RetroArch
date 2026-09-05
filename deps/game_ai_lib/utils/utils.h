// Adapted from OpenAI's retro source code:
// https://github.com/openai/retro
#pragma once

#include <string>
#include <vector>

namespace Retro {

enum class Operation {
	NOOP,
	EQUAL,
	NEGATIVE_EQUAL,
	NOT_EQUAL,
	LESS_THAN,
	GREATER_THAN,
	LESS_OR_EQUAL,
	GREATER_OR_EQUAL,
	NONZERO,
	ZERO,
	POSITIVE,
	NEGATIVE,
	SIGN,
};

int64_t calculate(Operation op, int64_t reference, int64_t value);

std::string drillUp(const std::vector<std::string>& targets, const std::string& fail = {}, const std::string& hint = ".");
}
