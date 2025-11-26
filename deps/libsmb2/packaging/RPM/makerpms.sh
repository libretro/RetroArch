#!/bin/sh
#
# makerpms.sh  -  build RPM packages from the git sources
#
# Copyright (C) John H Terpstra 1998-2002
# Copyright (C) Gerald (Jerry) Carter 2003
# Copyright (C) Jim McDonough 2007
# Copyright (C) Andrew Tridgell 2007
# Copyright (C) Michael Adam 2008-2009
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 3 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, see <http://www.gnu.org/licenses/>.
#

#
# The following allows environment variables to override the target directories
#   the alternative is to have a file in your home directory called .rpmmacros
#   containing the following:
#   %_topdir  /home/mylogin/redhat
#
# Note: Under this directory rpm expects to find the same directories that are under the
#   /usr/src/redhat directory
#

EXTRA_OPTIONS="$1"

DIRNAME=$(dirname $0)
TOPDIR=${DIRNAME}/../..

SPECDIR=`rpm --eval %_specdir`
SRCDIR=`rpm --eval %_sourcedir`

SPECFILE="libsmb2.spec"
SPECFILE_IN="libsmb2.spec.in"
RPMBUILD="rpmbuild"

# We use tags and determine the version, as follows:
# libsmb2-0.9.1  (First release of 0.9).
# libsmb2-0.9.23 (23rd minor release of the 112 version)
#
# If we're not directly on a tag, this is a devel release; we append
# .0.<patchnum>.<checksum>.devel to the release.
TAG=`git describe`
case "$TAG" in
    libsmb2-*)
	TAG=${TAG##libsmb2-}
	case "$TAG" in
	    *-*-g*) # 0.9-168-ge6cf0e8
		# Not exactly on tag: devel version.
		VERSION=`echo "$TAG" | sed 's/\([^-]\+\)-\([0-9]\+\)-\(g[0-9a-f]\+\)/\1.0.\2.\3.devel/'`
		;;
	    *)
		# An actual release version
		VERSION=$TAG
		;;
	esac
	;;
    *)
	echo Invalid tag "$TAG" >&2
	exit 1
	;;
esac

sed -e s/@VERSION@/$VERSION/g \
	< ${DIRNAME}/${SPECFILE_IN} \
	> ${DIRNAME}/${SPECFILE}

VERSION=$(grep ^Version ${DIRNAME}/${SPECFILE} | sed -e 's/^Version:\ \+//')

if echo | gzip -c --rsyncable - > /dev/null 2>&1 ; then
	GZIP="gzip -9 --rsyncable"
else
	GZIP="gzip -9"
fi

pushd ${TOPDIR}
echo -n "Creating libsmb2-${VERSION}.tar.gz ... "
mkdir -p "${SRCDIR}"
git archive --prefix=libsmb2-${VERSION}/ HEAD | ${GZIP} > ${SRCDIR}/libsmb2-${VERSION}.tar.gz
RC=$?
popd
echo "Done."
if [ $RC -ne 0 ]; then
        echo "Build failed!"
        exit 1
fi

# At this point the SPECDIR and SRCDIR variables must have a value!

##
## copy additional source files
##
mkdir -p ${SPECDIR}
cp -p ${DIRNAME}/${SPECFILE} ${SPECDIR}

##
## Build
##
echo "$(basename $0): Getting Ready to build release package"
${RPMBUILD} -ba --clean --rmsource ${EXTRA_OPTIONS} ${SPECDIR}/${SPECFILE} || exit 1

echo "$(basename $0): Done."

exit 0
