#
# Dmalloc RPM file for building of .rpm files for Redhat Linux systems.
#
# $Id: dmalloc.spec,v 1.10 2000/11/13 16:51:48 gray Exp $
#
Summary: Debug Malloc (Dmalloc)
Name: dmalloc
Version: 4.8.0
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
CFLAGS="${RPM_OPT_FLAGS}" ./configure --prefix=${RPM_BUILD_ROOT}/usr --enable-threads
make all
make light

%install
make install

%clean
rm -rf ${RPM_BUILD_ROOT}

%files
%defattr(444,root,root,755)

%doc INSTALL README NEWS
%doc dmalloc.info dmalloc.html dmalloc.texi

%attr(755,root,root) /usr/bin/dmalloc
/usr/include/dmalloc.h
/usr/lib/libdmalloc.a
/usr/lib/libdmallocxx.a
/usr/lib/libdmalloclp.a
/usr/lib/libdmallocth.a
/usr/lib/libdmallocthcxx.a
