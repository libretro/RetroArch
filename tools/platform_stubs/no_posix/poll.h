/* A target without POSIX headers (MSVC, consoles). compile-matrix.sh
 * builds the Linux- and BSD-only units against this directory: a unit
 * that reaches <poll.h> outside its platform gate fails here, as it
 * would on such a target. */
#error "<poll.h> included on a target that has none; gate the include with the unit's platform macro"
