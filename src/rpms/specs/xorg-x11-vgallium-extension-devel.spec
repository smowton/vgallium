# norootforbuild

Name: xorg-x11-server-vgallium-extension-devel
BuildRequires: automake, autoconf, make  
License: GPL
Group: Utilities/System
AutoReqProv: yes
Version: 1.0
Release: 0    
Summary: Headers for building apps that communicate with the Vgallium X extension
Url: http://www.cl.cam.ac.uk/~cs448/git
Source: opentc-repo-snapshot.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-build       

%description
These header files define the X protocol extension used by the Vgallium X extension, as well as the network protocol it employs to communicate with apps listening for window events. As of the time of writing this is needed to build the compositor, domU driver, and the X extension itself.

Authors:
--------
Christopher Smowton

%prep
%setup -q -n opentc-repo-snapshot

%build
cd progs/xen3d/xext/xen3dproto
./autogen.sh --prefix=/usr

%install
mkdir -p $RPM_BUILD_ROOT/usr/include
mkdir -p $RPM_BUILD_ROOT/usr/lib
cd progs/xen3d/xext/xen3dproto
make install DESTDIR=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%dir %{_includedir}/X11/extensions/
%{_includedir}/X11/extensions/xen3d_ext.h
%{_includedir}/X11/extensions/xen3d_extproto.h
/usr/lib/pkgconfig/xen3dproto.pc

%changelog
