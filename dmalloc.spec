#
# Dmalloc RPM file for building of .rpm files for Redhat Linux systems.
#
# $Id: dmalloc.spec,v 1.27 2003/11/18 15:28:29 gray Exp $
#
Summary: Debug Malloc (Dmalloc)
Name: dmalloc
Version: 5.2.4
Release: 1
Group: Development/Libraries
Copyright: public domain
URL: http://dmalloc.com/
Source: http://dmalloc.com/releases/%{name}-%{version}.tgz
BuildRoot: /var/tmp/%{name}-buildroot
Prefix: /usr

%description
The debug memory allocation or "dmalloc" library has been designed as
a drop in replacement for the system's `malloc', `realloc', `calloc',
`free' and other memory management routines while providing powerful
debugging facilities configurable at runtime.  These facilities
include such things as memory-leak tracking, fence-post write
detection, file/line number reporting, and general logging of
statistics.  It also provides support for the debugging of threaded
programs.  Releases and documentation available online. 
http://dmalloc.com/

%prep
%setup

%build
CFLAGS="${RPM_OPT_FLAGS}" ./configure --prefix=${RPM_BUILD_ROOT}/usr --enable-threads --enable-shlib --enable-cxx
# NOTE: we make the test program first because otherwise it screws up
# and tried to use the .so instead of the .a for some stupid reason
make dmalloc_t all light

%install
make install installcxx installth installsl

%clean
rm -rf ${RPM_BUILD_ROOT}

%files
%defattr(444,root,root,755)

%doc INSTALL README NEWS
%doc docs/dmalloc.info docs/dmalloc.html docs/dmalloc.texi

%attr(755,root,root) /usr/bin/dmalloc
/usr/include/dmalloc.h
/usr/lib/libdmalloc.a
/usr/lib/libdmalloc.so
/usr/lib/libdmallocxx.a
/usr/lib/libdmallocxx.so
/usr/lib/libdmallocth.a
/usr/lib/libdmallocth.so
/usr/lib/libdmallocthcxx.a
/usr/lib/libdmallocthcxx.so
