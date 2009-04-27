#
# spec file for package hermes (Version 1.3.3)
#
# Copyright (c) 2008 SUSE LINUX Products GmbH, Nuernberg, Germany.
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#
# Please submit bugfixes or comments via http://bugs.opensuse.org/
#

# norootforbuild

Name:           hermes
License:        LGPL
Group:          System/Libraries
Provides:       Hermes
Autoreqprov:    on
URL:            http://www.clanlib.org/hermes/
Version:        1.3.3
Release:        459.27
Summary:        A Graphics Conversion Library
Source:         Hermes-%{version}.tar.bz2
Patch0:         Hermes-%{version}-intptr_t.patch
Patch1:         Hermes-%{version}-destdir.patch
Patch2:         Hermes-%{version}-libtool.patch
Patch3:         Hermes-%{version}-gcc4.patch
Patch4:         Hermes-%{version}-noexecstack.patch
Patch5:         Hermes-%{version}-ar.diff
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
A graphics library for converting between various pixel formats.



Authors:
--------
    Christian Nentwich <c.nentwich@cs.ucl.ac.uk>
    Mikko Tiihonen <Mikko.Tiihonen@hut.fi>
    Glenn Fiedler <ptc@gaffer.org>

%package devel
Group:          Development/Libraries/C and C++
Summary:        Hermes library developer files
Requires:       hermes = %{version}

%description devel
This package contains files needed for development with the hermes
library.



Authors:
--------
    Christian Nentwich <c.nentwich@cs.ucl.ac.uk>
    Mikko Tiihonen <Mikko.Tiihonen@hut.fi>
    Glenn Fiedler <ptc@gaffer.org>

%prep
%setup -q -n Hermes-%{version}
%patch0
%patch1
%patch2
%patch3
%patch4
%patch5 -p1

%build
autoreconf --force --install
%ifarch %ix86
%configure
%else
%configure --disable-x86asm
%endif
make
rm -rf docs/api/{generate,sgml}

%install
mkdir -p $RPM_BUILD_ROOT%{_includedir}/Hermes
make DESTDIR=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root)
%doc AUTHORS ChangeLog COPYING FAQ INSTALL.unix NEWS README TODO*
%{_libdir}/libHermes.so.*

%files devel
%defattr(-,root,root)
%doc docs/api/*
%{_includedir}/Hermes
%{_libdir}/libHermes.so
%{_libdir}/libHermes.*a

%changelog
* Wed Mar 18 2009 Christopher Smowton
- fixed 'ar' error when building under 11.1
* Thu May 15 2008 prusnak@suse.cz
- added #include <stdint.h> to intptr_t.patch
* Wed Mar 19 2008 prusnak@suse.cz
- updated to 1.3.3
- dropped obsolete patch: uninitialized.patch
* Mon Jun 26 2006 nadvornik@suse.cz
- fixed uninitialized vars [#159135]
- compile asm sources with --noexecstack
* Wed Jan 25 2006 mls@suse.de
- converted neededforbuild to BuildRequires
* Mon Apr  4 2005 nadvornik@suse.cz
- fixed to compile with gcc4
* Sat Jan 10 2004 adrian@suse.de
- add %%defattr and %%run_ldconfig
* Wed May 28 2003 ro@suse.de
- add .la file to devel file list
* Wed May 14 2003 ro@suse.de
- hack to build with current libtool
* Fri Jul  5 2002 kukuk@suse.de
- Use %%ix86 macro
* Fri Jun 14 2002 meissner@suse.de
- full auto* rerun for ppc64
* Sat May 11 2002 schwab@suse.de
- Fix invalid cast.
- Fix alignment loops.
* Thu Apr 25 2002 ro@suse.de
- fix DESTDIR usage
* Wed Apr 24 2002 nadvornik@suse.cz
- used macro %%{_libdir}
* Wed Jun  6 2001 nadvornik@suse.cz
- created devel subpackage
- updated to 1.3.2
* Wed May 16 2001 nadvornik@suse.cz
- fixed cast warnings on ia64 (intptr_t.patch)
* Thu Apr 13 2000 nadvornik@suse.cz
- added BuildRoot
- update to 1.3.1
* Sat Apr  8 2000 bk@suse.de
- added suse update config macro
* Thu Feb 17 2000 sndirsch@suse.de
- created package hermes
