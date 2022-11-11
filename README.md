Debug Malloc Library
====================

Version 5.6.5 -- 12/28/2020

[![CircleCI](https://circleci.com/gh/j256/dmalloc.svg?style=svg)](https://circleci.com/gh/j256/dmalloc)

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
page](https://dmalloc.com/).

Enjoy.  Gray Watson

## Documentation

See the INSTALL.txt file for building, installation, and quick-start notes.

Examine the [html
documentation](https://htmlpreview.github.io/?https://raw.githubusercontent.com/j256/dmalloc/master/dmalloc.html) for
dmalloc.  The source of all documation is the dmalloc.texi texinfo file which also can generate PDF hardcopy output with
the help of the texinfo.tex file.  You can download the full documentation package or read it [online from the
repository](https://dmalloc.com/).

## Quick Getting Started

This section should give you an idea on how to get going.  See the more complete [getting started
documentation](https://dmalloc.com/docs/getting-started) for more details.

  1. Download the latest version of the library available from https://dmalloc.com/.

  2. Run `./configure` to configure the library.

  3. Run `make install` to install the library on your system.

  4. Add an alias for the dmalloc utility.  The idea is to have the shell capture the dmalloc
     program's output and adjust the environment.

     Bash, ksh, and zsh users should add the following to their dot files:

         function dmalloc { eval `command dmalloc -b $*`; }

     Csh or tcsh users  should add the following to their dot files:

         alias dmalloc 'eval `\dmalloc -C \!*`'

  5. Link the dmalloc library into your program and the end of the library list.

  8. Enable the debugging features by (for example) typing `dmalloc -l logfile -i 100 low`.
     Use `dmalloc --usage` to see other arguments to the dmalloc program.

  9. Run your program, examine the logfile, and use its information to help debug your program.

## Thanks

The initial idea of this library came from Doug Balog.  He and many other net folk contributed to the design,
development, and continued maintenence of the library.  My thanks goes out to them all.

# ChangeLog Release Notes

See the [ChangeLog.txt file](ChangeLog.txt).
