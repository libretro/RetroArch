#!/bin/sh
# The actions/cache key for apt-install.sh's cache: the job, the runner
# image and the packages asked for. A new runner image or another
# package list makes a new entry; the prefix lets one of the same job
# seed it, and apt-install.sh refreshes what no longer resolves.
#
#   apt-cache-key.sh "PACKAGE..."   (writes key= and prefix= to
#                                    GITHUB_OUTPUT)
set -eu
pkgs=$(printf '%s\n' $1 | sort -u | tr '\n' ' ')
hash=$(printf '%s' "$pkgs" | sha256sum | cut -c1-16)
prefix="apt-${GITHUB_JOB:-job}-${ImageOS:-unknown}-"
key="$prefix${ImageVersion:-unknown}-$hash"
echo "key=$key" >> "$GITHUB_OUTPUT"
echo "prefix=$prefix" >> "$GITHUB_OUTPUT"
echo "apt cache key: $key"
