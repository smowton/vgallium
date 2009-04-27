# norootforbuild

Name: vgallium-driver-softpipe           
BuildRequires: binutils, make, scons, xorg-x11-devel, libexpat-devel, gcc-c++, hermes-devel, xen-devel, xorg-x11-server-sdk
License: GPL
Group: Utilities/System
AutoReqProv: yes
Version: 0.9
Release: 0
Provides: vgallium-driver
Summary: Softpipe driver for xen3d compositor      
Url: http://www.cl.cam.ac.uk/~cs448/git
Source: opentc-repo-snapshot.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-build       

%description
The Gallium softpipe driver, built with the egl_xlib winsys layer and a thin state-tracker providing direct access to Gallium methods, for use with the Vgallium compositor application at present, and perhaps in the future other clients in need of raw Gallium access.

Authors:
--------
Christopher Smowton

%package -n vgallium-remoting-libgl
BuildRequires: binutils, make, scons, xorg-x11-devel, libexpat-devel, xorg-x11-server-vgallium-extension-devel, gntmem-module-devel
#Requirements which need expressing: needs the gntmem headers and the Xorg extension headers
License: GPL
Group: Utilities/System
AutoReqProv: yes
Version: 0.9
Release: 0    
Summary: Remoting version of libgl for vgallium guests
Url: http://www.cl.cam.ac.uk/~cs448/git

%description -n vgallium-remoting-libgl
A version of libGL.so which remotes Gallium drawing commands using interdomain shared memory, for use remoting 3D drawing with xen3d / vgallium.

%package -n xen3d-compositor
BuildRequires: xen-libs, xorg-x11-server-vgallium-extension-devel
License: GPL
Group: Utilities/System    
AutoReqProv: yes
Version: 0.9
Release: 0
Requires: vgallium-driver    
Summary: Performs composition of Vgallium clients and qemu-dm instances   
Url: http://www.cl.cam.ac.uk/~cs448/git

%description -n xen3d-compositor
Accepts XIDC connections from 3D apps running the vgallium remoting libGL, from Xorg extensions reporting their clipping information, and from a modified
qemu-dm providing 2D content relating to other VMs. These are composed to produce hardware-accelerated graphics for guest VMs.

%package -n xen3d-domu-daemon
BuildRequires: xen-libs  
License: GPL
Group: Utilities/System    
AutoReqProv: yes
Version: 1.0
Release: 0
Summary: Helper utility for Xen interdomain communication using /dev/gntmem

%description -n xen3d-domu-daemon
Helper utility for Xen interdomain communication using /dev/gntmem and event channels

%prep
%setup -q -n opentc-repo-snapshot

%build
export CFLAGS="$RPM_OPT_FLAGS -fno-strict-aliasing -Wall"
scons statetrackers=raw drivers=softpipe winsys=egl_xlib dri=no
scons statetrackers=mesa drivers=remote winsys=remote_xlib dri=no
make linux-x86
cd progs/xen3d
chmod 755 config.sh
./config.sh x86 
mkdir ./obj
make
cd xen3dd
make

%install
mkdir -p $RPM_BUILD_ROOT/usr/lib
mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT/usr/lib/vgallium
cp -ax build/linux-x86/lib/librawgal.* $RPM_BUILD_ROOT/usr/lib/.
cp -ax build/linux-x86/lib/libGL.* $RPM_BUILD_ROOT/usr/lib/vgallium/.
cp -ax lib/libEGL.* $RPM_BUILD_ROOT/usr/lib/.
cp progs/xen3d/xen3d $RPM_BUILD_ROOT/usr/bin/.
cp progs/xen3d/xen3dd/xen3dd $RPM_BUILD_ROOT/usr/bin/.

%clean
rm -rf $RPM_BUILD_ROOT

%post
ldconfig

%postun
ldconfig

%files
%defattr(-,root,root)
%{_libdir}/librawgal.*

%files -n vgallium-remoting-libgl
%defattr(-,root,root)
%{_libdir}/vgallium
%{_libdir}/vgallium/libGL.*

%files -n xen3d-compositor
%defattr(-,root,root)
%{_bindir}/xen3d
%{_libdir}/libEGL.*

%files -n xen3d-domu-daemon
%defattr(-,root,root)
%{_bindir}/xen3dd

%changelog

