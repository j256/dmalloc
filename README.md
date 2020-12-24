Debug Malloc Library
====================

Version 5.6.4 -- 12/23/2020

[![CircleCI](https://circleci.com/gh/j256/dmalloc.svg?style=svg)](https://circleci.com/gh/j256/dmalloc)
![CI](https://github.com/j256/dmalloc/workflows/CI/badge.svg)

The debug memory allocation or "dmalloc" library has been designed as a drop in replacement for the system's
`malloc`, `realloc`, `calloc`, `free` and other memory management routines while providing powerful debugging
facilities configurable at runtime.  These facilities include such things as memory-leak tracking, fence-post
write detection, file/line number reporting, and general logging of statistics.

The library is reasonably portable having been run successfully on at least the following operating systems:
AIX, DGUX, Free/Net/OpenBSD, GNU/Hurd, HPUX, Irix, Linux, OSX, NeXT, OSF/DUX, SCO, Solaris, Sunos, Ultrix,
Unixware, MS Windows, and Unicos on a Cray T3E.  It also provides support for the debugging of threaded
programs.

The package includes the library, configuration scripts, debug utility application, test program, and
documentation.  Online documentation as well as the full source is available at the [dmalloc home
page](http://dmalloc.com/).

Enjoy, Gray Watson

## Documentation

See the INSTALL.txt file for building, installation, and quick-start notes.

Examine the dmalloc.html file which contains the user-documentation for the dmalloc subsystem.  The source of
all documation is the dmalloc.texi texinfo file which also can generate PDF hardcopy output with the help of
the texinfo.tex file.  You can download the full documentation package or read it
[online from the repository](http://dmalloc.com/).

## Thanks

The initial idea of this library came from Doug Balog.  He and many other net folk contributed to the design,
development, and continued maintenence of the library.  My thanks goes out to them all.

# ChangeLog Release Notes

See the [ChangeLog.txt file](ChangeLog.txt).
