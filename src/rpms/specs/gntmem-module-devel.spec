# norootforbuild

Name: gntmem-module-devel
License: GPL
Group: Utilities/System
AutoReqProv: yes
Version: 1.0
Release: 0    
Summary: Header for gntmem kernel module
Url: http://www.cl.cam.ac.uk/~cs448/git
Source: opentc-repo-snapshot.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-build       

%description
Header file describing IOCTLs for communication with the gntmem kernel module

Authors:
--------
Christopher Smowton

%prep
%setup -q -n opentc-repo-snapshot

%install
mkdir -p $RPM_BUILD_ROOT/usr/include/xen/sys
cp progs/xen3d/kernel/gntmem.h $RPM_BUILD_ROOT/usr/include/xen/sys/gntmem.h

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%dir %{_includedir}/xen/
%dir %{_includedir}/xen/sys/
%{_includedir}/xen/sys/gntmem.h

%changelog
