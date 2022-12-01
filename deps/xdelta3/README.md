Xdelta 3.x readme.txt
Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008,
2009, 2010, 2011, 2012, 2013, 2014, 2015
<josh.macdonald@gmail.com>


Thanks for downloading Xdelta!

This directory contains the Xdelta3 command-line interface (CLI) and source
distribution for VCDIFF differential compression, a.k.a. delta
compression. The latest information and downloads are available here:

  http://xdelta.org/
  http://github.com/jmacd/xdelta/

Xdelta can be configured to use XZ Utils for secondary compression:

  http://tukaani.org/xz/

The command-line syntax is detailed here:

  https://github.com/jmacd/xdelta/blob/wiki/CommandLineSyntax.md

Run 'xdelta3 -h' for brief help.  Run 'xdelta3 test' for built-in tests.

Sample commands (like gzip, -e means encode, -d means decode)

  xdelta3 -9 -S lzma -e -f -s OLD_FILE NEW_FILE DELTA_FILE
  xdelta3 -d -s OLD_FILE DELTA_FILE DECODED_FILE

File bug reports and browse open support issues here:

  https://github.com/jmacd/xdelta/issues

The source distribution contains the C/C++/Python APIs, Unix, Microsoft VC++
and Cygwin builds.  Xdelta3 is covered under the terms of the APL, see
LICENSE.
