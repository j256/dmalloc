#
# Process this file with autoconf to produce a configure script.
#
# NOTE: the beginning comments should be #'ed so we get the #!/bin/sh
# at the very top of the configure script.
#
# Copyright 2000 by Gray Watson
#
# This file is part of the dmalloc package.
#
# Permission to use, copy, modify, and distribute this software for
# any purpose and without fee is hereby granted, provided that the
# above copyright notice and this permission notice appear in all
# copies, and that the name of Gray Watson not be used in
# advertising or publicity pertaining to distribution of the
# document or software without specific, written prior permission.
#
# Gray Watson makes no representations about the suitability of the
# software described herein for any purpose.  It is provided "as is"
# without express or implied warranty.
#
# The author may be contacted via http://dmalloc.com/
#
# $Id: configure.ac,v 1.5 2003/05/21 15:59:46 gray Exp $
#
AC_REVISION($Revision: 1.5 $)
AC_INIT(dmalloc, 5.0.1, [http://dmalloc.org/])
AC_CONFIG_SRCDIR(dmalloc.c)
AC_CONFIG_HEADER(conf.h)

AC_MSG_NOTICE([configurations for the dmalloc library])

##############################################################################
# arguments

AC_ARG_ENABLE(cxx,
    [ AC_HELP_STRING([--disable-cxx],
		[do not build in C++ support (enabled by default)] ) ],
    [ # enable_cxx set by default ],
    [ enable_cxx=yes ]
)
AC_ARG_ENABLE(threads,
    [ AC_HELP_STRING([--enable-threads],
		[build in thread support (disabled by default)] ) ],
    [ # enable_threads set by default ],
    [ enable_threads=no ]
)
AC_ARG_ENABLE(shlib,
    [ AC_HELP_STRING([--enable-shlib],
		[build shared libraries (disabled by default)] ) ],
    [ # enable_shlib set by default ],
    [ enable_shlib=no ]
)

##############################################################################
AC_MSG_NOTICE([build utilities])

AC_PROG_CC
AC_PROG_CXX

# hopefully we have a stdc c-compiler
if test "$if ac_cv_prog_cc_stdc" = "no" ; then
	AC_MSG_WARN()
	AC_MSG_WARN(WARNING: no ansi compiler.  this build may fail.)
	AC_MSG_WARN()
fi
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_C_CONST

# we need this for various settings
AC_HEADER_STDC
AC_CHECK_HEADER([stdarg.h],
		[AC_SUBST([HAVE_STDARG_H],1)])
AC_CHECK_HEADER([stdlib.h],
		[AC_SUBST([HAVE_STDLIB_H],1)])
AC_CHECK_HEADER([string.h],
		[AC_SUBST([HAVE_STRING_H],1)])
AC_CHECK_HEADER([unistd.h],
		[AC_SUBST([HAVE_UNISTD_H],1)])
AC_CHECK_HEADER([sys/types.h],
		[AC_SUBST([HAVE_SYS_TYPES_H],1)])
AC_CHECK_HEADER([sys/mman.h],
		[AC_SUBST([HAVE_SYS_MMAN_H],1)])

###############################################################################
# shared library handling

#
# check to see where we should install our shared libraries
# DONT CACHE in case the change the --prefix
#
# This is pretty tricky here and should be handled by autoconf
# automatically.  I try to see if there is a shlib directory
# to install by shlibs there.  If not I use the $libdir.  Some
# of the variables have values of '${exec_prefix}' and the like
# so the quoting is very important.
#
# We first try and find the true directory where we are
# installing.
#
if test "x$exec_prefix" = xNONE; then
	if test "x$prefix" = xNONE; then
		shlibprefix="$ac_default_prefix"
	else
		shlibprefix="$prefix"
	fi
else
	shlibprefix="$exec_prefix"
fi
#
# Then we see if there is a special shlib directory to use.
#
if test -d $shlibprefix/shlib; then
	shlibdir='${exec_prefix}/shlib'
else
	shlibdir="$libdir"
fi
AC_SUBST(shlibdir)

# see how the local system builds shared libraries
AC_MSG_CHECKING([shared library link args])
AC_COMPILE_IFELSE([ int foo(int val) { return val + 1; } ],[
	# so now we try to create an archive from the compiled .o file
	(ar cr conftest.a conftest.o) 2>&5
	# see which shared-library ld commands work
	if (ld -shared --whole-archive -soname conftest.so -o conftest.so.t conftest.a) 2>&5; then
		ac_cv_shared_link_args='ld -shared --whole-archive -soname $@ -o $@.t'
	elif (ld -shared -o conftest.so.t -all -soname conftest.so.t -none -lc -all conftest.a) 2>&5; then
		ac_cv_shared_link_args='ld -shared -o $@.t -all -soname $@ -none -lc -all'
	elif (ld -G -o conftest.so.t conftest.a) 2>&5; then
		ac_cv_shared_link_args='ld -G -o $@.t'
	else
		# oh well, toss an error
		ac_cv_shared_link_args='# Could not configure shlib linking'
		enable_shlib=no
	fi
],[
	# oh well, toss an error
	ac_cv_shared_link_args='# Could not configure shlib linking'
	enable_shlib=no
])
shlinkargs="$ac_cv_shared_link_args"
AC_MSG_RESULT([$ac_cv_shared_link_args])
AC_SUBST(shlinkargs)

###############################################################################

AC_MSG_CHECKING([C++ support])
if test "$enable_cxx" = "yes"; then
	AC_SUBST([CXX_ON],[])
	AC_SUBST([CXX_OFF],[#])
	AC_MSG_RESULT([enabled])
else
	AC_SUBST([CXX_ON],[#])
	AC_SUBST([CXX_OFF],[])
	AC_MSG_RESULT([disabled])
fi

AC_MSG_CHECKING([thread support])
if test "$enable_threads" = "yes"; then
	AC_SUBST([TH_ON],[])
	AC_SUBST([TH_OFF],[#])
	AC_MSG_RESULT([enabled])
else
	AC_SUBST([TH_ON],[#])
	AC_SUBST([TH_OFF],[])
	AC_MSG_RESULT([disabled])
fi

# shlib support
AC_MSG_CHECKING(shlib support)
if test "$enable_shlib" = "yes"; then
	AC_SUBST([SL_ON],[])
	AC_SUBST([SL_OFF],[#])
	AC_MSG_RESULT(enabled)
else
	AC_SUBST([SL_ON],[#])
	AC_SUBST([SL_OFF],[])
	AC_MSG_RESULT(disabled)
fi

###############################################################################
#
# check for size_t
#
AC_TYPE_SIZE_T
if test "$ac_cv_type_size_t" = "yes"; then
	AC_DEFINE(DMALLOC_SIZE,size_t)
else
	AC_DEFINE(DMALLOC_SIZE,unsigned long)
fi

#
# check for unsigned-ness
#
AC_MSG_CHECKING([dmalloc size unsigned])
AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <sys/types.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
main() { DMALLOC_SIZE x = -1; if (x >= 0) exit(0); else exit(1); }
]])],
[AC_DEFINE(DMALLOC_SIZE_UNSIGNED,1)  AC_MSG_RESULT([yes])],
[AC_DEFINE(DMALLOC_SIZE_UNSIGNED,0)  AC_MSG_RESULT([no])],
[AC_DEFINE(DMALLOC_SIZE_UNSIGNED,0)  AC_MSG_RESULT([no])]
)

#
# check for strdup macro (linux)
#
AC_MSG_CHECKING([strdup macro])
AC_RUN_IFELSE([AC_LANG_SOURCE([[
#if HAVE_STDLIB_H
#include <string.h>
#endif

#ifdef strdup
main() { exit(0); }
#else
main() { exit(1); }
#endif
]])],
[ac_cv_strdup_macro=yes],
[ac_cv_strdup_macro=no],
[ac_cv_strdup_macro=no]
)
AC_MSG_RESULT([$ac_cv_strdup_macro])

#
# check for existance of mprotect and associated defines
#
AC_CHECK_FUNCS(mprotect)
AC_MSG_CHECKING([mprotect works])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/mman.h>
]],[[
  int prot = PROT_NONE | PROT_READ | PROT_WRITE;
  (void)mprotect(0, 0, prot);
]])],
[ AC_DEFINE(PROTECT_ALLOWED,1) AC_MSG_RESULT([yes]) ],
[ AC_DEFINE(PROTECT_ALLOWED,0) AC_MSG_RESULT([no]) ]
)

##############################################################################
AC_MSG_NOTICE([important functionality])

AC_CHECK_FUNCS(sbrk)
if test $ac_cv_func_sbrk = no; then
	AC_MSG_WARN()
	AC_MSG_WARN(WARNING: no sbrk function.  See INTERNAL_MEMORY_SPACE in settings.dist.)
	AC_MSG_WARN()
	AC_DEFINE(HEAP_GROWS_UP, 1)
else
	#
	# heap growing up
	#
	AC_MSG_CHECKING([heap grows])
	AC_RUN_IFELSE([
		#define SBRK_AMOUNT	(32 * 1024)
		main() {
		  char *first, *next;
		  first = (char *)sbrk(SBRK_AMOUNT);
		  next = (char *)sbrk(SBRK_AMOUNT);
		  if (next > first)
		    _exit(0);
		  else
		    _exit(1);
		}
	],
	[ AC_DEFINE(HEAP_GROWS_UP, 1) AC_MSG_RESULT([up]) ],
	[ AC_DEFINE(HEAP_GROWS_UP, 0) AC_MSG_RESULT([down]) ],
	[ AC_DEFINE(HEAP_GROWS_UP, 1) AC_MSG_RESULT([up]) ]
	)
fi

#
# check for basic block size
#
AC_CHECK_FUNCS(getpagesize)
AC_MSG_CHECKING([basic-block size])
ac_cv_page_size=0
if test $ac_cv_page_size = 0; then
   AC_RUN_IFELSE([main() { if (getpagesize()<=2048) exit(0); else exit(1); }],
	[ ac_cv_page_size=11 ] )
fi
if test $ac_cv_page_size = 0; then
   AC_RUN_IFELSE([main() { if (getpagesize()<=4096) exit(0); else exit(1); }],
	[ ac_cv_page_size=12 ] )
fi
if test $ac_cv_page_size = 0; then
   AC_RUN_IFELSE([main() { if (getpagesize()<=8192) exit(0); else exit(1); }],
	[ ac_cv_page_size=13 ] )
fi
if test $ac_cv_page_size = 0; then
   AC_RUN_IFELSE([main() { if (getpagesize()<=16384) exit(0); else exit(1); }],
	[ ac_cv_page_size=14 ] )
fi
if test $ac_cv_page_size = 0; then
    ac_cv_page_size=15
fi
AC_DEFINE_UNQUOTED([BASIC_BLOCK],[$ac_cv_page_size])
AC_MSG_RESULT([$ac_cv_page_size])

#
# data-alignment size...
#
AC_CHECK_SIZEOF(long)
AC_MSG_CHECKING([data-alignment size])
if test $ac_cv_sizeof_long = 4; then
	# we have to make a special case for sun sparc idiocy
	ac_cv_data_align=8
else
	ac_cv_data_align=$ac_cv_sizeof_long
fi
AC_DEFINE_UNQUOTED([ALLOCATION_ALIGNMENT],[$ac_cv_data_align])
AC_MSG_RESULT([$ac_cv_data_align])

#
# check the safety of the abort function
#
AC_CHECK_FUNCS(abort)
AC_MSG_CHECKING([abort safe])
AC_RUN_IFELSE([[
static int main_b = 0;
static char heap_mem[102400], *heap_p = heap_mem;
free () { if (main_b) _exit(0); }
char *malloc (int size) {
  char *pnt;
  if (main_b) _exit(0);
  pnt = heap_p;
  heap_p += size;
  return pnt;
}
char *calloc (int number, int size) {
  char *start, *pnt, *end;
  if (main_b) _exit(0);
  /* it should be already 0s */
  start = malloc (number * size);
  pnt = start;
  end = start + size;
  while (pnt < end) { *pnt++ = '\0'; }
  return start;
}
char *realloc (char *old_pnt, int new_size) {
  char *start, *pnt, *end;
  if (main_b) _exit(0);
  start = malloc (new_size);
  pnt = start;
  end = start + new_size;
  while (pnt < end) { *pnt++ = *old_pnt++; }
  return start;
}
main() { main_b = 1; abort(); _exit(1); }
]],
# NOTE: this is reversed because abort core dumps and doesn't exit with
# 0 which means that it should fail
[ AC_DEFINE(ABORT_OKAY, 0) AC_MSG_RESULT([no]) ],
# if it succeeds then something called free on the way out
[ AC_DEFINE(ABORT_OKAY, 1) AC_MSG_RESULT([yes]) ],
[ AC_DEFINE(ABORT_OKAY, 0) AC_MSG_RESULT([no]) ]
)

AC_TYPE_SIGNAL
AC_MSG_CHECKING([signal works])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
	#include <signal.h>
]], [[
	(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGTERM, SIG_IGN);
]])],
[ AC_DEFINE(SIGNAL_OKAY, 1) AC_MSG_RESULT([yes]) ],
[ AC_DEFINE(SIGNAL_OKAY, 0) AC_MSG_RESULT([no]) ]
)

##############################################################################

#
# check if the return.h macros work
#
AC_MSG_CHECKING([return.h macros work])
AC_RUN_IFELSE([

#define __CONF_H__
#define USE_RETURN_MACROS 1
#define RETURN_MACROS_WORK 1

#include "return.h"

static void foo (void)
{
  char	*ret_addr;
  GET_RET_ADDR(ret_addr);
}

main()
{
  foo();
  exit(0);
}
],
[ AC_DEFINE(RETURN_MACROS_WORK, 1) AC_MSG_RESULT([yes]) ],
[ AC_DEFINE(RETURN_MACROS_WORK, 0) AC_MSG_RESULT([no]) ],
[ AC_DEFINE(RETURN_MACROS_WORK, 0) AC_MSG_RESULT([no]) ]
)

##############################################################################

#
# check to see if ident workds
#
AC_MSG_CHECKING([if ident works])
AC_COMPILE_IFELSE([
#ident "$Id: configure.ac,v 1.5 2003/05/21 15:59:46 gray Exp $"
static void foo (void) { }
],
[AC_DEFINE(IDENT_WORKS, 1) AC_MSG_RESULT([yes])],
[AC_DEFINE(IDENT_WORKS, 0) AC_MSG_RESULT([no])]
)

##############################################################################
AC_MSG_NOTICE([pthread particulars])

XX_OLD_LIBS=$LIBS

AC_SEARCH_LIBS(pthread_mutex_init, pthread pthreads c_r)
AC_CHECK_HEADERS(pthread.h pthreads.h)

AC_CHECK_FUNCS(pthread_mutex_init pthread_mutex_lock pthread_mutex_unlock)

AC_MSG_CHECKING([pthread mutex type])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#if HAVE_PTHREAD_H
#include <pthread.h>
#endif
#if HAVE_PTHREADS_H
#include <pthreads.h>
#endif
]],[[
   pthread_mutex_t dmalloc_mutex;
]])],
[AC_DEFINE(THREAD_MUTEX_T,pthread_mutex_t) AC_MSG_RESULT([pthread_mutex_t])],
[AC_MSG_RESULT([unknown])]
)

AC_MSG_CHECKING([pthread attribute initialization])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#if HAVE_PTHREAD_H
#include <pthread.h>
#endif
#if HAVE_PTHREADS_H
#include <pthreads.h>
#endif
]], [[
	THREAD_MUTEX_T dmalloc_mutex;
	pthread_mutex_init(&dmalloc_mutex, pthread_mutexattr_default);
]])],
[AC_DEFINE(THREAD_LOCK_INIT_VAL,pthread_mutexattr_default)
 AC_MSG_RESULT([pthread_mutexattr_default])],
[AC_DEFINE(THREAD_LOCK_INIT_VAL,0)
 AC_MSG_RESULT([0])]
)

LIBS=$XX_OLD_LIBS

##############################################################################
AC_MSG_NOTICE([functions])

# auto-shutdown functions
AC_CHECK_FUNCS(atexit on_exit)
if test $ac_cv_func_atexit = no && test $ac_cv_func_on_exit = no; then
	AC_MSG_WARN()
	AC_MSG_WARN(WARNING: The library cannot automatically shut itself)
	AC_MSG_WARN(down without atexit or on_exit.  You will need to call)
	AC_MSG_WARN(dmalloc_shutdown directly before exit to get the final)
	AC_MSG_WARN(statistics and unfreed memory information.)
	AC_MSG_WARN()
fi

# other bells and whistles
AC_CHECK_FUNCS(fork getpid time ctime random srandom)
AC_CHECK_FUNCS(vprintf snprintf vsnprintf)
AC_CHECK_FUNCS(recalloc memalign valloc)

# required
AC_CHECK_FUNCS(memcmp memcpy memset)
AC_CHECK_FUNCS(strchr strrchr)
AC_CHECK_FUNCS(strlen strcmp strcpy strsep)

##############################################################################
AC_MSG_NOTICE([various functions for argv files])

# NOTE: rest handled above
AC_CHECK_FUNCS(strncmp strncpy)

##############################################################################
AC_MSG_NOTICE([various functions for argument checking])

AC_CHECK_FUNCS(bcmp bcopy bzero)
AC_CHECK_FUNCS(memccpy memchr)
# we have the escape index because it is a m4/autoconf command
AC_CHECK_FUNCS([[index]] rindex strcasecmp strncasecmp)
AC_CHECK_FUNCS(strcat strdup strspn strcspn strncat strpbrk strstr strtok)

AC_CONFIG_FILES(Makefile)

AC_CONFIG_COMMANDS([dmalloc.h.2],
[
newfile=dmalloc.h.2
rm -f $newfile.t
echo '/* this is dmalloc.h.2 */' > $newfile.t
echo '/* produced by configure, inserted into dmalloc.h */' >> $newfile.t
echo '' >> $newfile.t

if test "$ac_cv_c_const" = "yes"; then
	echo '/* const is available */' >> $newfile.t
else
        echo '/* const is not available */' >> $newfile.t
	echo '#ifndef const' >> $newfile.t
	echo '#define const' >> $newfile.t
	echo '#endif' >> $newfile.t
fi

if test "$ac_cv_strdup_macro" = "yes"; then
	echo '/* strdup is a macro */' >> $newfile.t
	echo '#define DMALLOC_STRDUP_MACRO' >> $newfile.t
else
	echo '/* strdup is not a macro */' >> $newfile.t
	echo '#undef DMALLOC_STRDUP_MACRO' >> $newfile.t
fi
echo '' >> $newfile.t

echo '/*' >> $newfile.t
echo ' * the definition of DMALLOC_SIZE' >> $newfile.t
echo ' *' >> $newfile.t
echo ' * NOTE: some architectures have malloc, realloc, etc.' >> $newfile.t
echo ' * using unsigned instead of unsigned long.  You may' >> $newfile.t
echo ' * have to edit this by hand to fix any compilation' >> $newfile.t
echo ' * warnings or errors.' >> $newfile.t
echo ' */' >> $newfile.t
if test "$ac_cv_type_size_t" = "yes"; then
	echo "#include <sys/types.h>" >> $newfile.t
	echo "#define DMALLOC_SIZE size_t" >> $newfile.t
else
	echo "/* no include file needed */" >> $newfile.t
	echo "#define DMALLOC_SIZE unsigned long" >> $newfile.t
fi
echo '' >> $newfile.t
echo '/*' >> $newfile.t
echo ' * We use stdarg.h for the dmalloc_message and' >> $newfile.t
echo ' * dmalloc_vmessage functions.' >> $newfile.t
echo ' */' >> $newfile.t
if test "$ac_cv_header_stdarg_h" = "yes"; then
	echo "#include <stdarg.h>" >> $newfile.t
	echo "#define DMALLOC_STDARG 1" >> $newfile.t
else
	echo "/* no stdarg.h available */" >> $newfile.t
	echo "#define DMALLOC_STDARG 0" >> $newfile.t
fi
echo '' >> $newfile.t
if cmp -s $newfile $newfile.t 2>/dev/null; then
	AC_MSG_NOTICE([$newfile is unchanged])
	rm -f $newfile.t
else
	rm -f $newfile
	mv $newfile.t $newfile
fi
],
[
# initialization commands
ac_cv_c_const=$ac_cv_c_const
ac_cv_strdup_macro=$ac_cv_strdup_macro
ac_cv_type_size_t=$ac_cv_type_size_t
ac_cv_header_stdarg_h=$ac_cv_header_stdarg_h
])

AC_CONFIG_COMMANDS([settings.h],
[
newfile=settings.h
rm -f $newfile.t
echo '/*' > $newfile.t
echo ' * WARNING: this file was produced from settings.dist' >> $newfile.t
echo ' * by the configure program.  The configure script, when' >> $newfile.t
echo ' * run again, will overwrite changed made here.' >> $newfile.t
echo ' */' >> $newfile.t
echo '' >> $newfile.t
cat $srcdir/settings.dist >> $newfile.t

if cmp -s $newfile.t $newfile 2>/dev/null; then
	AC_MSG_NOTICE([$newfile is unchanged])
	rm -f $newfile.t
else
	rm -f $newfile
	mv $newfile.t $newfile
fi
])
AC_OUTPUT

AC_MSG_NOTICE([])
AC_MSG_NOTICE([Please check-out Makefile and conf.h to make sure that])
AC_MSG_NOTICE([sane configuration values were a result.])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([You may want to change values in settings.h before])
AC_MSG_NOTICE([running 'make'.])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([To run the basic library tests, you can execute:])
AC_MSG_NOTICE([  make light])
AC_MSG_NOTICE([or])
AC_MSG_NOTICE([  make heavy])
AC_MSG_NOTICE([])
