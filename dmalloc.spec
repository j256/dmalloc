#
# Dmalloc RPM file for building of .rpm files for Redhat Linux systems.
#
# $Id: dmalloc.spec,v 1.1 2000/05/08 12:20:58 gray Exp $
#
Summary: Debug Malloc (Dmalloc)
Name: dmalloc
URL: http://dmalloc.com/
%define version 4.5.2
Version: %{version}
Release: 1
Copyright: public domain
Group: Development/Libraries
Source: http://dmalloc.com/releases/dmalloc-%{version}.tar.gz
BuildRoot: /var/tmp/dmalloc-buildroot
Prefix: /usr
Conflicts: dmalloc < %{version}

%description
The debug memory allocation or "dmalloc" library has been designed as
a drop in replacement for the system's `malloc', `realloc', `calloc',
`free' and other memory management routines while providing powerful
debugging facilities configurable at runtime.  These facilities
include such things as memory-leak tracking, fence-post write
detection, file/line number reporting, and general logging of
statistics.  It also provides support for the debugging of threaded
programs.  Releases and documentation available online. http://dmalloc.com/

%prep
%setup

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=/usr
make all threads

%install
make installprefix="$RPM_BUILD_ROOT" install installth

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(644,root,root,755)

%doc INSTALL README NEWS
%doc dmalloc.info dmalloc.html dmalloc.texi

%attr(755,root,root) /usr/bin/dmalloc
/usr/include/dmalloc.h
/usr/lib/libdmalloc.a
/usr/lib/libdmalloclp.a
/usr/lib/libdmallocth.a
