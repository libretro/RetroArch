#if __TEST_FNMATCH__
#include <assert.h>
#endif

#include "rl_fnmatch.h"

// Implemnentation of fnmatch(3) so it can be distributed to non *nix platforms
// No flags are implemented ATM. We don't use them. Add flags as needed.
int rl_fnmatch(const char *pattern, const char *string, int flags) {
	const char *c;
	int charmatch = 0;
	int rv;
	for (c = pattern; *c != '\0'; c++) {
		// String ended before pattern
		if ((*c != '*') && (*string == '\0')) {
			return FNM_NOMATCH;
		}

		switch (*c) {
		// Match any number of unknown chars
		case '*':
			// Find next node in the pattern ignoring multiple
			// asterixes
			do {
				c++;
				if (*c == '\0') {
					return 0;
				}
			} while (*c == '*');

			// Match the remaining pattern ingnoring more and more
			// chars.
			do {
				// We reached the end of the string without a
				// match. There is a way to optimize this by
				// calculating the minimum chars needed to
				// match the remaining pattern but I don't
				// think it is worth the work ATM.
				if (*string == '\0') {
					return FNM_NOMATCH;
				}

				rv = rl_fnmatch(c, string, flags);
				string++;
			} while (rv != 0);

			return 0;
		// Match char from list
		case '[':
			charmatch = 0;
			for (c++; *c != ']'; c++) {
				// Bad formath
				if (*c == '\0') {
					return FNM_NOMATCH;
				}

				// Match already found
				if (charmatch) {
					continue;
				}

				if (*c == *string) {
					charmatch = 1;
				}
			}

			// No match in list
			if (!charmatch) {
				return FNM_NOMATCH;
			}

			string++;
			break;
		// Has any char
		case '?':
			string++;
			break;
		// Match following char verbatim
		case '\\':
			c++;
			// Dangling escape at end of pattern
			if (c == '\0') {
				return FNM_NOMATCH;
			}
		default:
			if (*c != *string) {
				return FNM_NOMATCH;
			}
			string++;
		}
	}

	// End of string and end of pattend
	if (*string == '\0') {
		return 0;
	} else {
		return FNM_NOMATCH;
	}
}

#if __TEST_FNMATCH__
int main() {
	assert(rl_fnmatch("TEST", "TEST", 0) == 0);
	assert(rl_fnmatch("TE?T", "TEST", 0) == 0);
	assert(rl_fnmatch("TE[Ssa]T", "TEST", 0) == 0);
	assert(rl_fnmatch("TE[Ssda]T", "TEsT", 0) == 0);
	assert(rl_fnmatch("TE[Ssda]T", "TEdT", 0) == 0);
	assert(rl_fnmatch("TE[Ssda]T", "TEaT", 0) == 0);
	assert(rl_fnmatch("TEST*", "TEST", 0) == 0);
	assert(rl_fnmatch("TEST**", "TEST", 0) == 0);
	assert(rl_fnmatch("TE*ST*", "TEST", 0) == 0);
	assert(rl_fnmatch("TE**ST*", "TEST", 0) == 0);
	assert(rl_fnmatch("TE**ST*", "TExST", 0) == 0);
	assert(rl_fnmatch("TE**ST", "TEST", 0) == 0);
	assert(rl_fnmatch("TE**ST", "TExST", 0) == 0);
	assert(rl_fnmatch("TE\\**ST", "TE*xST", 0) == 0);
	assert(rl_fnmatch("*.*", "test.jpg", 0) == 0);
	assert(rl_fnmatch("*.jpg", "test.jpg", 0) == 0);
	assert(rl_fnmatch("*.[Jj][Pp][Gg]", "test.jPg", 0) == 0);
	assert(rl_fnmatch("*.[Jj]*[Gg]", "test.jPg", 0) == 0);
	assert(rl_fnmatch("TEST?", "TEST", 0) == FNM_NOMATCH);
	assert(rl_fnmatch("TES[asd", "TEST", 0) == FNM_NOMATCH);
	assert(rl_fnmatch("TEST\\", "TEST", 0) == FNM_NOMATCH);
	assert(rl_fnmatch("TEST*S", "TEST", 0) == FNM_NOMATCH);
	assert(rl_fnmatch("TE**ST", "TExT", 0) == FNM_NOMATCH);
	assert(rl_fnmatch("TE\\*T", "TExT", 0) == FNM_NOMATCH);
	assert(rl_fnmatch("TES?", "TES", 0) == FNM_NOMATCH);
	assert(rl_fnmatch("TE", "TEST", 0) == FNM_NOMATCH);
	assert(rl_fnmatch("TEST!", "TEST", 0) == FNM_NOMATCH);
	assert(rl_fnmatch("DSAD", "TEST", 0) == FNM_NOMATCH);
}
#endif
