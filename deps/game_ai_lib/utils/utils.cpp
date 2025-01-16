// Adapted from OpenAI's retro source code:
// https://github.com/openai/retro
#include "utils.h"

#include <climits>
#include <cstdlib>
#include <sys/stat.h>

using namespace std;

namespace Retro {

int64_t calculate(Operation op, int64_t reference, int64_t value) {
	switch (op) {
	case Operation::NOOP:
		return value;
	case Operation::EQUAL:
		return value == reference;
	case Operation::NEGATIVE_EQUAL:
		return value == -reference;
	case Operation::NOT_EQUAL:
		return value != reference;
	case Operation::LESS_THAN:
		return value < reference;
	case Operation::GREATER_THAN:
		return value > reference;
	case Operation::LESS_OR_EQUAL:
		return value <= reference;
	case Operation::GREATER_OR_EQUAL:
		return value >= reference;
	case Operation::NONZERO:
		return value != 0;
	case Operation::ZERO:
		return value == 0;
	case Operation::POSITIVE:
		return value > 0;
	case Operation::NEGATIVE:
		return value < 0;
	case Operation::SIGN:
		return value < 0 ? -1 : value > 0 ? 1 : 0;
	}
	return 0;
}

string drillUp(const vector<string>& targets, const string& fail, const string& hint)
{
	#if 0
	char rpath[PATH_MAX];
	string path(".");
#ifndef _WIN32
	if (!hint.empty() && realpath(hint.c_str(), rpath)) {
		path = rpath;
	}
#else
	if (!hint.empty()) {
		path = hint;
	}
#endif
	while (!path.empty() && path != "/") {
		for (const auto& target : targets) {
			struct stat statbuf;
			string testPath = path + "/" + target;
			if (stat(testPath.c_str(), &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
				return testPath;
			}
		}
#ifndef _WIN32
		string new_path = path.substr(0, path.find_last_of('/'));
#else
		string new_path = path.substr(0, path.find_last_of("/\\"));
#endif
		if (new_path == path) {
			break;
		}
		path = new_path;
#ifndef _WIN32
		if (!path.empty() && realpath(path.c_str(), rpath)) {
			path = rpath;
		}
#endif
	}
	if (!fail.empty()) {
		return fail + "/" + targets[0];
	}
	return {};

#else
return {};
#endif
}
}
