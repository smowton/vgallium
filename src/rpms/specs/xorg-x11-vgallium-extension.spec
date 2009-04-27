#
# spec file for package xorg-x11-server (Version 7.4)
#
# Copyright (c) 2008 SUSE LINUX Products GmbH, Nuernberg, Germany.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#

# norootforbuild


Name:           xorg-x11-vgallium-extension
%define _unpackaged_files_terminate_build 0
%define dirsuffix 1.5.2
%define fglrx_driver_hack 0
%define vnc 1
BuildRequires:  Mesa-devel bison flex fontconfig-devel freetype2-devel ghostscript-library libdrm-devel libopenssl-devel pkgconfig xorg-x11 xorg-x11-devel xorg-x11-libICE-devel xorg-x11-libSM-devel xorg-x11-libX11-devel xorg-x11-libXau-devel xorg-x11-libXdmcp-devel xorg-x11-libXext-devel xorg-x11-libXfixes-devel xorg-x11-libXmu-devel xorg-x11-libXp-devel xorg-x11-libXpm-devel xorg-x11-libXprintUtil-devel xorg-x11-libXrender-devel xorg-x11-libXt-devel xorg-x11-libXv-devel xorg-x11-libfontenc-devel xorg-x11-libxkbfile-devel xorg-x11-proto-devel xorg-x11-xtrans-devel xorg-x11-server-vgallium-extension-devel gntmem-module-devel
%if %vnc
BuildRequires:  libjpeg-devel
%endif
Url:            http://xorg.freedesktop.org/
%define EXPERIMENTAL 0
Version:        7.4
Release:        17.3
License:        X11/MIT
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Group:          System/X11/Servers/XF86_4
Requires:       pkgconfig xorg-x11-fonts-core xorg-x11 xkeyboard-config
%if %suse_version > 1010
%ifnarch s390 s390x
Requires:       xorg-x11-driver-input xorg-x11-driver-video
%endif
%endif
Provides:       xorg-x11-server-glx
Obsoletes:      xorg-x11-server-glx
Summary:        X.Org Server
Source:         xorg-server-%{dirsuffix}.tar.bz2
Source3:        README.updates
Source4:        xorgcfg.tar.bz2
%if %suse_version > 1010
Source5:        modprobe.nvidia
%endif
Source7:        xorg-docs-1.4.tar.bz2
Source8:        xorg.conf.man-070818.tar
Patch:          64bit.diff
Patch1:         fpic.diff
Patch2:         p_default-module-path.diff
Patch6:         pu_fixes.diff
Patch7:         p_mouse_misc.diff
Patch8:         p_bug96328.diff
Patch11:        ps_showopts.diff
Patch13:        p_xorg_acpi.diff
Patch14:        p_xkills_wrong_client.diff
Patch16:        p_xnest-ignore-getimage-errors.diff
Patch18:        p_ia64-console.diff
Patch22:        disable-root-xorg_conf.diff
Patch23:        disable-fbblt-opt.diff
Patch27:        mouse.diff
Patch29:        xephyr.diff
Patch32:        acpi_events.diff
Patch34:        p_pci-off-by-one.diff.ia64
Patch36:        libdrm.diff
%if %vnc
### Dan Nicholson <dbn.lists@gmail.com>
#http://people.freedesktop.org/~dbn/xorg-server-xf4vnc.patch
Patch39:        xorg-server-xf4vnc.patch
Patch40:        xorg-server-xf4vnc-disable-dmxvnc.diff
Patch42:        xorg-server-xf4vnc-TranslateNone.diff
Patch43:        xorg-server-xf4vnc-abi-version.diff
Patch44:        xorg-server-xf4vnc-cutpaste.diff
Patch46:        xorg-server-xf4vnc-busyloop.diff
%endif
Patch41:        loadmod-bug197195.diff
Patch45:        bug-197858_dpms.diff
Patch63:        xorg-x11-server-1.2.99-unbreak-domain.patch
Patch67:        xorg-docs.diff
Patch72:        randr12-8d230319040f0a7f72231da2bf5ec97dc3612e21.diff
Patch77:        fbdevhw.diff
Patch79:        edit_data_sanity_check.diff
Patch83:        ia64linuxPciInit.diff
Patch93:        pixman.diff
Patch101:       zap_warning_xserver.diff
Patch103:       confine_to_shape.diff
Patch104:       bitmap_always_unscaled.diff
Patch106:       randr1_1-sig11.diff
Patch109:       events.diff
Patch112:       fix-dpi-values.diff
Patch113:       no-return-in-nonvoid-function.diff
Patch114:       64bit-portability-issue.diff
Patch117:       acpi-warning.diff
Patch118:       exa-greedy.diff
Patch120:       dga_cleanup.diff
Patch121:       miPointerUpdate-crashfix.diff
Patch122:       unplugged_monitor_crashfix.diff
Patch123:       vidmode-sig11.diff
Patch124:       commit-59f9fb4b8.diff
Patch125:       0001-Xinput-Catch-missing-configlayout-when-deleting-dev.patch
Patch126:       commit-a9e2030.diff
Patch127:       dpms_screensaver.diff
Patch128:       otc-xorg.diff

%description
This package contains the Xen3D / Vgallium X extension

%prep
%setup -q -n xorg-server-%{dirsuffix} -a4 -a7 -a8
%patch
%patch1
%patch2
%patch6
%patch7 -p2
%patch8 -p0
%patch11
%patch13
%patch14
%patch16 -p2
pushd hw/xfree86/os-support
%patch18
popd
%patch22
%patch23
%patch27
%patch29
### Bug 197572: X.Org PCI/IA64 patches
%patch32 -p1
### FIXME
#%patch34 -p0
%patch36 -p0
%if %vnc
%patch39 -p1
%patch40 -p0
%patch42 -p1
%patch43 -p0
%patch44
%patch46 -p1
chmod 755 hw/vnc/symlink-vnc.sh
%endif
%patch41 -p1
%patch45 -p0
### FIXME
#%patch63 -p1
pushd xorg-docs-*
%patch67
popd
%patch72 -p1
%patch77
%patch79 -p1
### FIXME
#%patch83
%patch93
%patch101 -p1
%patch103
%patch104 -p1
%patch106 -p1
%patch109 -p1
%patch112 -p0
%patch113 -p0
%patch114 -p0
%patch117
%patch118 -p1
%patch120 -p1
%patch121 -p0
%patch122 -p0
%patch123 -p0
%patch124 -p1
%patch125 -p1
%patch126 -p1
%patch127 -p1
%patch128 -p1

%build
pushd xorg-docs-*
autoreconf -fi
export CFLAGS="$RPM_OPT_FLAGS -fno-strict-aliasing"
%configure
make
popd
autoreconf -fi
# DRI2 disabled for Xserver 1.5 as libdrm 2.3.1 doesn't have the
# drmBO functionality.
./configure CFLAGS="$RPM_OPT_FLAGS -fno-strict-aliasing" \
%if %fglrx_driver_hack
            --with-release-major=7 \
            --with-release-minor=2 \
            --with-release-patch=0 \
            --with-release-snap=0 \
            --with-release-date="%(date)" \
            --with-release-version=7.2.0.0 \
%endif
%if %vnc
            --enable-vnc \
            --disable-xcliplist \
%endif
            --prefix=/usr \
	    --sysconfdir=/etc \
            --libdir=%{_libdir} \
            --mandir=%{_mandir} \
            --enable-builddocs \
            --enable-install-libxf86config \
%ifarch %EXPERIMENTAL
            --enable-glx-tls \
            --enable-multibuffer \
%endif
%ifarch s390 s390x
            --disable-aiglx \
%else
            --enable-aiglx \
%endif
            --enable-lbx \
            --enable-xdmcp \
            --enable-xdm-auth-1 \
            --enable-dri \
            --disable-dri2 \
%ifarch s390 s390x
            --disable-xorg \
%else
            --enable-xorg \
%endif
            --enable-dmx \
            --enable-xnest \
            --enable-kdrive \
            --enable-xephyr \
            --disable-xsdl \
            --enable-xprint \
            --disable-kbd_mode \
            --disable-xprint \
            --enable-record \
            --with-log-dir="/var/log" \
            --with-os-name="openSUSE" \
            --with-os-vendor="SUSE LINUX" \
            --with-fontdir="/usr/share/fonts" \
            --with-xkb-path="/usr/share/X11/xkb" \
            --with-xkb-output="/var/lib/xkb/compiled"
#make %{?jobs:-j %jobs}
make
make -C hw/kdrive %{?jobs:-j %jobs}

%install
make install DESTDIR=$RPM_BUILD_ROOT

%clean
rm -rf "$RPM_BUILD_ROOT"

%files
%defattr(-,root,root)
%dir /usr/lib/xorg
%dir /usr/lib/xorg/modules
%dir /usr/lib/xorg/modules/extensions
/usr/lib/xorg/modules/extensions/libxen3dext.la
/usr/lib/xorg/modules/extensions/libxen3dext.so

%changelog
* Fri Nov 28 2008 sndirsch@suse.de
- dpms_screensaver.diff
  * DMPS calls dixSaveScreens() when turned on but not when turned
    off. In most cases this is irrelevant as DPMS is done when a
    key is hit in which case dixSaveScreens() will be called to
    unblank anyhow. This isn't the case if we use xset (or the
    DPMS extension directly) to unblank. (bnc #439495)
* Wed Nov 26 2008 sndirsch@suse.de
- rename "i810" driver entry in xorg.conf to "intel" during update
  (bnc #448458)
* Fri Nov 21 2008 sndirsch@suse.de
- commit-a9e2030.diff
  * int10: Do an mprotect(..,PROT_EXEC) on shmat()ed memory
    ranges. When the linux kernel sets the NX bit vm86 segfaults
    when it tries to execute code in memory that is not marked
    EXEC. Such code gets called whenever we return from a VBIOS
    call to signal the calling program that the call is actually
    finished and that we are not trapping for other reasons
    (like IO accesses). Use mprotect(2) to set these memory
    ranges PROT_EXEC. (bnc #443440)
* Thu Nov 13 2008 sndirsch@suse.de
- 0001-Xinput-Catch-missing-configlayout-when-deleting-dev.patch
  * In DeleteInputDeviceRequest (xf86Xinput.c), we access idev
    members even if idev is null. This takes down the xserver
    hard in some cases (kernel SIGABRT), and segfaults on other
    cases (Luc Verhaegen).
* Sat Nov  8 2008 sndirsch@suse.de
- commit-59f9fb4b8.diff
  * XAA PixmapOps: Sync before accessing unwrapped callbacks.
    (bnc #435791)
- obsoletes XAA_pixmap_sync.diff
* Fri Nov  7 2008 sndirsch@suse.de
- XAA_pixmap_sync.diff
  * By adding a line with SYNC_CHECK to the XAA_PIXMAP_OP_PROLOGUE
    macro, all XAA pixmap callbacks now properly wait for the
    hardware to be synced before calling the (next) unwrapped
    callback. This effectively clears up all the drawing issues
    we are seeing. (bnc #435791)
* Thu Nov  6 2008 sndirsch@suse.de
- vidmode-sig11.diff
  * fixes Sig11 in vidmode extension (bnc #439354)
* Wed Nov  5 2008 sndirsch@suse.de
- unplugged_monitor_crashfix.diff
  * prevent monitor from crashing during startup if statically
    configured external has been unplugged (bfo #18246)
* Tue Nov  4 2008 sndirsch@suse.de
- removed glitz-devel from BuildRequires (bnc #441549)
* Mon Oct 27 2008 sndirsch@suse.de
- build and install libxf86config + header files also on s390(x)
  (bnc #432738)
* Mon Oct 27 2008 sndirsch@suse.de
- removed p_ppc_domain_workaround.diff/ppc.diff to fix Xserver
  start on ppc (bnc #437695)
* Sat Oct 25 2008 sndirsch@suse.de
- xorg-server-xf4vnc-busyloop.diff
  * prevent Xvnc from busylooping when client disconnects
  (bnc #403901)
* Fri Oct 17 2008 sndirsch@suse.de
- miPointerUpdate-crashfix.diff
  * fixes Xserver crash at startup with ELO touchscreen
    (bnc #436435)
* Sat Oct 11 2008 sndirsch@suse.de
- xorg-server 1.5.2
  * int10: Remove useless check.
  * int10: Don't warn when scanning for devices we don't have.
  * int10: Fix a nasty memory leak.
  * Revert "Array-index based devPrivates implementation."
  * EDID: Catch monitors that encode aspect ratio for physical
    size.
  * Remove usage of mfbChangeWindowAttributes missed in
    e4d11e58c...
  * only build dri2 when DRI2 is enabled
  * Array-index based devPrivates implementation.
  * Fix GKVE with key_code > 255
  * xkb: fix use of uninitialized variable.
  * DGA: Fix ProcXF86DGASetViewPort for missing support in driver.
  * xkb: fix core keyboard map generation. (bfo #14373)
  * xkb: squash canonical types into explicit ones on core
    reconstruction.
  * Check nextEnabledOutput()'s return in bestModeForAspect()
- obsoletes xorg-server-commit-d1bb5e3.diff
* Fri Oct 10 2008 sndirsch@suse.de
- dga_cleanup.diff
  * DGA: Mash together xf86dga.c and xf86dga2.c for a client state
    tracking fix.
  * DGA: Track client state even when using old style DGA. This
    fixes the issue that a badly killed DGA will keep on hogging
    mode/framebuffer/mouse/keyboard. (bnc #310232)
* Thu Oct  9 2008 sndirsch@suse.de
- xorg-server-commit-d1bb5e3.diff
  * DGA: Fix ProcXF86DGASetViewPort for missing support in driver.
    Fixes a segfault when trying to activate a DGA mode without
    checking whether DGA modesetting is at all possible.
    (Luc Verhaegen)
* Mon Sep 29 2008 sndirsch@suse.de
- make use of %%configure macro
* Tue Sep 23 2008 sndirsch@suse.de
- xorg-server 1.5.1 (planned for final X.Org 7.4 release)
  * Conditionalize Composite-based backing store on
    pScreen->backingStoreSupport. (Aaron Plattner)
  * Move RELEASE_DATE below AC_INIT. (Adam Jackson)
  * exa: disable shared pixmaps (Julien Cristau)
  * Fix panoramiX request and reply swapping (Peter Harris)
* Mon Sep 22 2008 sndirsch@suse.de
- disabled build of optional "xcliplist" module (bnc #428189)
* Sat Sep 13 2008 sndirsch@suse.de
- added /usr/lib64/X11 dir to filelist to fix build on 64bit
  platforms
* Thu Sep 11 2008 sndirsch@suse.de
- bumped release number to 7.4
* Thu Sep  4 2008 sndirsch@suse.de
- xorg-server 1.5.0
  * almost certainly the server that will go into Xorg 7.4,
    which is supposed to be available in a day or two
- obsoletes commit-5930aeb.diff/commit-78f50cd.diff
* Thu Aug 28 2008 sndirsch@suse.de
- commit-5930aeb.diff/commit-78f50cd.diff
  * obsoletes reverting of Mesa commit 1724334 (bfo #17069)
* Fri Aug  8 2008 sndirsch@suse.de
- commit-50e80c3.diff obsolete now (bnc #415680)
- commit-f6401f9.diff obsolete
* Wed Aug  6 2008 schwab@suse.de
- Fix crash in Xvnc when handling selections.
* Tue Aug  5 2008 sndirsch@suse.de
- enabled build of record extension, which has been disabled
  upstream for whatever reason
* Fri Aug  1 2008 sndirsch@suse.de
- xorg-server-xf4vnc-abi-version.diff
  * raised ABI version for xorg-server 1.5(-pre)
* Thu Jul 24 2008 sndirsch@suse.de
- xorg-server 1.4.99.906
- obsoletes commit-a18551c.diff
* Tue Jul 22 2008 sndirsch@suse.de
- exa-greedy.diff
  * Make sure exaMigrateTowardFb/Sys end up calling exaCopyDirty
    (bfo #16773)
* Fri Jul 18 2008 schwab@suse.de
- Kill useless warning.
* Mon Jul 14 2008 sndirsch@suse.de
- improved ppc/ppc64 patch once more
* Fri Jul 11 2008 sndirsch@suse.de
- improved ppc/ppc64 patch
- Xvfb (xorg-x11-server-extra) requires Mesa (swrast_dri.so) now
* Thu Jul 10 2008 sndirsch@suse.de
- xorg-server-xf4vnc-TranslateNone.diff
  * supposed to fix Xvnc crash when VNC client is running on a
    display with the same color depth (bnc #389386)
- ppc.diff
  * fixes build on ppc/ppc64
* Thu Jul 10 2008 sndirsch@suse.de
- enabled build of Xvnc/libvnc
- xorg-server-xf4vnc-disable-dmxvnc.diff
  * disabled VNC feature in DMX to fix VNC build
* Thu Jul 10 2008 sndirsch@suse.de
- updated to new vnc patch "xorg-server-xf4vnc.patch "by Dan
  Nicholson, which is still disabled due to build errors
- obsoletes the following patches:
  * xorg-server-1.4-vnc-64bit.diff
  * xorg-server-1.4-vnc-disable_render.diff
  * xorg-server-1.4-vnc-fix.patch
  * xorg-server-1.4-vnc-memory.diff
  * xorg-server-1.4-vnc-render_sig11.diff
  * xorg-server-1.4-vnc.patch
* Tue Jul  8 2008 sndirsch@suse.de
- commit-a18551c.diff
  * Fix GLX in Xvfb and kdrive.
    Xvfb could no longer be started: "Xvfb: symbol lookup error:
    /usr/lib/dri/swrast_dri.so: undefined symbol:
    _glapi_add_dispatch". This is fixed now.
- removed no longer appliable patch 'p_xf86Mode.diff'
* Fri Jul  4 2008 sndirsch@suse.de
- xorg-server-1.4.99.905
  * obsolete patches
  - XAANoOffscreenPixmaps.diff
  - bug227111-ddc_screensize.diff
  - commit-184e571.diff
  - commit-29e0e18.diff
  - commit-c6c284e.diff
  - commit-f7dd0c7.diff
  - commit-fa19e84.diff
  - commit-feac075.diff
  - glx-align.patch
  - mfb_without_xorg.diff
  - p_ValidatePci.diff
  - p_vga-crashfix.diff
  - xkb_action.diff
  - xorg-server.diff
  - xprint.diff
  - xserver-mode-fuzzy-check.diff
  * new patches
  - 64bit-portability-issue.diff
  - no-return-in-nonvoid-function.diff
  * adjusted patches
  - bitmap_always_unscaled.diff
  - disable-root-xorg_conf.diff
  - p_ppc_domain_workaround.diff
  - pixman.diff
  - ps_showopts.diff
  - xorg-server-1.4-vnc.patch
  - bug-197858_dpms.diff
- Mesa sources no longer required for xorg-server 1.5
- VNC patches + build disabled for now
- disabled some IA64 patches for now
* Fri Jun 13 2008 sndirsch@suse.de
- xorg-x11-Xvnc: added meta file for SuSEfirewall2 (bnc #398855)
* Wed Jun 11 2008 sndirsch@suse.de
- xorg-server 1.4.2
  * CVE-2008-2360 - RENDER Extension heap buffer overflow
  * CVE-2008-2361 - RENDER Extension crash
  * CVE-2008-2362 - RENDER Extension memory corruption
  * CVE-2008-1377 - RECORD and Security extensions memory corruption
  * CVE-2008-1379 - MIT-SHM arbitrary memory read
- obsoletes bfo-bug15222.diff
* Tue Jun 10 2008 sndirsch@suse.de
- xorg-server 1.4.1
  * Contains a few security and input fixes, some memory leak
    fixes, and a few misc bits.
  * obsolete patches:
  - CVE-2007-5760-xf86misc.diff
  - CVE-2007-6427-xinput.diff
  - CVE-2007-6428-TOG-cup.diff
  - CVE-2007-6429-shm_evi.diff
  - CVE-2008-0006-pcf_font.diff
  - commit-37b1258.diff
  - commit-a6a7fad.diff
  - remove_bogus_modeline.diff
  - xserver-1.3.0-xkb-and-loathing.patch
  * adjusted patches
  - xorg-server-1.4-vnc.patch
* Thu Jun  5 2008 sndirsch@suse.de
- bfo-bug15222.diff (bfo #15222, bnc #374318)
  * CVE-2008-2360 - RENDER Extension heap buffer overflow
  * CVE-2008-2361 - RENDER Extension crash
  * CVE-2008-2362 - RENDER Extension memory corruption
  * CVE-2008-1379 - MIT-SHM arbitrary memory read
  * CVE-2008-1377 - RECORD and Security extensions memory corruption
* Tue May 27 2008 sndirsch@suse.de
- xserver-mode-fuzzy-check.diff
  * Make mode checking more tolerant like in pre-RandR times.
* Mon May 26 2008 sndirsch@suse.de
- fix-dpi-values.diff
  * fixes DPI values for RANDR 1.2 capable drivers (bnc #393001)
* Fri May 16 2008 sndirsch@suse.de
- mention ZapWarning also in Xorg manual page (bnc #391352)
* Fri May 16 2008 sndirsch@suse.de
- xorg-server-1.4-vnc-render_sig11.diff
  * fixed sig11 in RENDER code (bnc #385677)
* Wed May 14 2008 sndirsch@suse.de
- disabled patch to disable RENDER support in Xvnc, since it broke
  24bit color depth support (bnc #390011)
* Mon May  5 2008 sndirsch@suse.de
- xorg-server-1.4-vnc-disable_render.diff
  * disabled RENDER support in Xvnc (bnc #385677)
* Mon Apr 21 2008 sndirsch@suse.de
- events.diff
  * eating up key events before going into the idle loop upon vt
    switch instead of after return (bnc #152522)
* Mon Apr 21 2008 sndirsch@suse.de
- xkb_action.diff
  * fixed remaining unitialized warning in X.Org (bnc #83910)
* Sun Apr 20 2008 sndirsch@suse.de
- fbdevhw.diff
  * screen blanking not supported by vesafb of Linux kernel
    (bnc #146462)
* Tue Apr 15 2008 sndirsch@suse.de
- no longer disable AIGLX by default
* Thu Apr 10 2008 sndirsch@suse.de
- XAANoOffscreenPixmaps.diff
  * disable Offscreen Pixmaps by default (bnc #376068)
* Wed Apr  9 2008 schwab@suse.de
- Fix another o-b-1 in pci domain support.
* Wed Apr  9 2008 sndirsch@suse.de
- randr1_1-sig11.diff
  * fixes Xserver crash when running xrandr on a different virtual
    terminal (Egbert Eich, bnc #223459)
* Mon Apr  7 2008 sndirsch@suse.de
- commit-37b1258.diff
  * possibly fixes unwanted autorepeat (bnc #377612, bfo #14811)
* Sat Apr  5 2008 sndirsch@suse.de
- bitmap_always_unscaled.diff
  * Default bitmap fonts should typically be set as unscaled (libv)
* Sat Apr  5 2008 sndirsch@suse.de
- update to Mesa bugfix release 7.0.3 (final) sources
* Wed Apr  2 2008 sndirsch@suse.de
- update to Mesa bugfix release 7.0.3 RC3 sources
* Mon Mar 31 2008 sndirsch@suse.de
- confine_to_shape.diff
  * fixes XGrabPointer's confine_to with shaped windows (bnc #62146)
* Thu Mar 20 2008 sndirsch@suse.de
- zap_warning_xserver.diff
  * implements FATE #302988: ZapWarning (Luc Verhaegen)
    Uses PCSpeaker for beep. Press once, beep. Press again within
    2s (which is ample), terminate. Documented in xorg.conf manpage.
- make the memory corruption fix by schwab a seperate patch to make
  sure it won't get lost the next time I update the VNC patch
* Wed Mar 19 2008 schwab@suse.de
- Fix vnc server memory corruption.
* Fri Mar  7 2008 sndirsch@suse.de
- commit-a6a7fad.diff
  * Don't break grab and focus state for a window when redirecting
    it. (bnc #336219, bfo #488264)
* Fri Feb 22 2008 sndirsch@suse.de
- update to Mesa bugfix release 7.0.3 RC2 sources
  * Fixed GLX indirect vertex array rendering bug (14197)
  * Fixed crash when deleting framebuffer objects (bugs 13507,
    14293)
  * User-defined clip planes enabled for R300 (bug 9871)
  * Fixed glBindTexture() crash upon bad target (bug 14514)
  * Fixed potential crash in glDrawPixels(GL_DEPTH_COMPONENT) (bug
    13915)
  * Bad strings given to glProgramStringARB() didn't generate
    GL_INVALID_OPERATION
  * Fixed minor point rasterization regression (bug 11016)
* Mon Feb  4 2008 sndirsch@suse.de
- added Requires:xkeyboard-config to xorg-x11-server
* Fri Feb  1 2008 sndirsch@suse.de
- commit-50e80c3.diff:
  * never overwrite realInputProc with enqueueInputProc
    (bnc#357989, bfo#13511)
* Thu Jan 24 2008 sndirsch@suse.de
- only switch to radeon driver in %%post if radeonold driver is no
  longer available (Bug #355009)
- some more cleanup in %%post
* Thu Jan 24 2008 schwab@suse.de
- Move manpage to the sub package that provides the binary.
* Wed Jan 23 2008 sndirsch@suse.de
- update to Mesa bugfix release 7.0.3 RC1 sources
  * Added missing glw.pc.in file to release tarball
  * Fix GLUT/Fortran issues
  * GLSL gl_FrontLightModelProduct.sceneColor variable wasn't
    defined
  * Fix crash upon GLSL variable array indexes (not yet supported)
  * Two-sided stencil test didn't work in software rendering
  * Fix two-sided lighting bugs/crashes (bug 13368)
  * GLSL gl_FrontFacing didn't work properly
  * glGetActiveUniform returned incorrect sizes (bug 13751)
  * Fix several bugs relating to uniforms and attributes in GLSL
    API (Bruce Merry, bug 13753)
  * glTexImage3D(GL_PROXY_TEXTURE_3D) mis-set teximage depth field
* Mon Jan 21 2008 sndirsch@suse.de
-  updated patch for CVE-2007-6429 once more (X.Org Bug #13520)
  * Always test for size+offset wrapping.
* Sun Jan 20 2008 sndirsch@suse.de
- updated patch for CVE-2007-6429 (Bug #345131)
  * Don't spuriously reject <8bpp shm pixmaps.
    Move size validation after depth validation, and only validate
    size if the bpp of the pixmap format is > 8.  If bpp < 8 then
    we're already protected from overflow by the width and height
    checks.
* Fri Jan 18 2008 sndirsch@suse.de
- X.Org security update
  * CVE-2007-5760 - XFree86 Misc extension out of bounds array index
  * CVE-2007-6427 - Xinput extension memory corruption.
  * CVE-2007-6428 - TOG-cup extension memory corruption.
  * CVE-2007-6429 - MIT-SHM and EVI extensions integer overflows.
  * CVE-2008-0006 - PCF Font parser buffer overflow.
* Wed Dec 12 2007 sndirsch@suse.de
- xorg-server 1.4.0.90 (prerelease of 1.4.1)
* Fri Nov 30 2007 sndirsch@suse.de
- pixman.diff
  * fixed include path for pixman.h
* Thu Nov 29 2007 sndirsch@suse.de
- remove_bogus_modeline.diff
  * remove bogus monitor modelines provided by DDC (Bug #335540)
* Tue Nov 27 2007 sndirsch@suse.de
- commit-184e571.diff
  * Adjust offsets of modes that do not fit virtual screen size.
- commit-c6c284e.diff
  * Initialize Mode with 0 in xf86RandRModeConvert.
- commit-f6401f9.diff
  * Don't segfault if referring to a relative output where no modes survived.
- commit-f7dd0c7.diff
  * Only clear crtc of output if it is the one we're actually working on.
- commit-fa19e84.diff
  * Fix initial placement of LeftOf and Above.
* Thu Nov 22 2007 sndirsch@suse.de
- pixman.diff no longer required
* Sun Nov 18 2007 sndirsch@suse.de
- s390(x): allow mfb build without Xorg server being built
* Thu Nov 15 2007 sndirsch@suse.de
- commit-29e0e18.diff
  * Make config file preferred mode override monitor preferred
    mode.
- commit-feac075.diff
  * Leave hardware-specified preferred modes alone when user
    preference exists.
- obsoletes preferred_mode-fix.diff
* Thu Nov 15 2007 sndirsch@suse.de
- added xorg-x11-fonts-core/xorg-x11 to Requires (Bug #341312)
* Wed Nov 14 2007 schwab@suse.de
- ia64linuxPciInit: allocate extra space for fake devices.
* Sat Nov 10 2007 sndirsch@suse.de
- updated to Mesa 7.0.2 (final) sources
* Wed Oct 31 2007 sndirsch@suse.de
- updated to Mesa 7.0.2 RC1 sources
* Tue Oct 23 2007 sndirsch@suse.de
- xorg-server-1.4-vnc-64bit.diff
  * fixes segfault on 64bit during Xserver start; make sure to
    define _XSERVER64 by having HAVE_DIX_CONFIG_H defined and
    therefore including dix-config.h, so Atom is CARD32 instead of
    unsigned long before and no longer messes up the pInfo structure
    in xf86rfbMouseInit/xf86rfbKeybInit
- finally enabled build of xf4vnc (standalone Xvnc and VNC Xserver
  module)
* Fri Oct 19 2007 sndirsch@suse.de
- updated xf4vnc patch; still disabled due to problematic vnc module
* Tue Oct  9 2007 sndirsch@suse.de
- preferred_mode-fix.diff
  * more reasonable patch (Bug #329724)
* Thu Oct  4 2007 sndirsch@suse.de
- preferred_mode-fix.diff
  * fixed endless loop if PreferredMode is set (Bug #329724)
* Wed Oct  3 2007 sndirsch@suse.de
- removed obsolete patch p_pci-domain.diff (Bug #308693, comment #26)
- apply p_pci-off-by-one.diff.ia64 on all platforms since it clearly
  only affects platforms, where INCLUDE_XF86_NO_DOMAIN is *not* set;
  this still not explains why we have seen Xserver hangups with the
  patch in place on at least some %%ix86/x86_64 machines with fglrx/
  nvidia driver IIRC; it needs to verified if this problem is still
  reproducable ... (Bug #308693, comment #25)
* Wed Oct  3 2007 sndirsch@suse.de
- xserver-1.3.0-xkb-and-loathing.patch
  * Ignore (not just block) SIGALRM around calls to Popen()/Pclose().
    Fixes a hang in openoffice when opening menus. (Bug #245711)
* Wed Oct  3 2007 sndirsch@suse.de
- added missing ia64Pci.h; required for IA64
* Wed Oct  3 2007 sndirsch@suse.de
- recreated p_pci-off-by-one.diff.ia64; the default fuzz factor of
  patch (2) resulted in a hunk applied to the wrong function and
  therefore broke the build :-(
* Fri Sep 28 2007 sndirsch@suse.de
- xorg-server 1.4
  * Welcome to X.Org X Server 1.4, now with hotplugging input to go
    with the hotplugging output.  Also included in this release are
    many performance and correctness fixes to the EXA acceleration
    architecture, support for DTrace profiling of the X Server,
    accelerated GLX_EXT_texture_from_pixmap with supporting DRI
    drivers, and many improvements to the RandR 1.2 support that
    was added in xorg-server-1.3. The X Server now relies on the
    pixman library, which replaces the fb/fbcompose.c and
    accelerated implementations that were previously shared through
    code-duplication with the cairo project.
  * obsolete patches:
  - bug-259290_trapfault.diff
  - cfb8-undefined.diff
  - commit-c09e68c
  - i810_dri_fix_freeze.diff
  - p_bug159532.diff
  - p_enable-altrix.diff
  - p_pci-ce-x.diff
  - p_pci-off-by-one.diff
  - p_xorg_rom_read.diff
  - randr12-2926cf1da7e4ed63573bfaecdd7e19beb3057d9b.diff
  - randr12-5b424b562eee863b11571de4cd0019cd9bc5b379.diff
  - randr12-aec0d06469a2fa7440fdd5ee03dc256a68704e77.diff
  - randr12-b2dcfbca2441ca8c561f86a78a76ab59ecbb40e4.diff
  - randr12-b4193a2eee80895c5641e77488df0e72a73a3d99.diff
  - remove__GLinterface.patch
  - support_mesa6.5.3.patch
  - use-composite-for-unequal-depths.patch
  - x86emu.diff
  - xephyr-sig11-fix.diff
  * adjusted patches:
  - 64bit.diff
  - bug-197858_dpms.diff
  - bug227111-ddc_screensize.diff
  - disable-root-xorg_conf.diff
  - fpic.diff
  - glx-align.patch
  - libdrm.diff
  - p_bug96328.diff
  - p_ia64-console.diff
  - p_vga-crashfix.diff
  - xephyr.diff
- pixman.diff:
  * search for pixman instead of pixman-1
- bumped version to 7.3
* Tue Sep 25 2007 sndirsch@suse.de
- remove wrongly prebuilt xf1bpp files after extracting tarball;
  fixes vga module loading (Bug #328201)
- do not use "make -j" to (quick)fix xf1bpp build
- do not apply p_pci-domain.diff on IA64
- use updated off-by-one patch by schwab for IA64
* Fri Sep 21 2007 sndirsch@suse.de
- edit_data_sanity_check.diff:
  * added sanity check for monitor EDID data (Bug #326454)
* Tue Sep 11 2007 sndirsch@suse.de
- reverted changes by schwab on Fri Sep 7; these resulted i a black
  screen during Xserver start with any driver on non-IA64 platforms
* Mon Sep 10 2007 sndirsch@suse.de
- use-composite-for-unequal-depths.patch:
  * Use Composite when depths don't match (Bug #309107, X.Org Bug
    [#7447])
* Fri Sep  7 2007 schwab@suse.de
- Update off-by-one patch.
- Remove empty patch.
* Mon Sep  3 2007 sndirsch@suse.de
- fbdevhw.diff:
  * ignore pixclock set to 0 by Xen kernel (Bug #285523)
* Fri Aug 31 2007 sndirsch@suse.de
- added several RANDR 1.2 fixes (Bug #306699)
  * randr12-2926cf1da7e4ed63573bfaecdd7e19beb3057d9b.diff
    Allocate the right number of entries for saving crtcs
  * randr12-5b424b562eee863b11571de4cd0019cd9bc5b379.diff
    Set the crtc before the output change is notified. Set the new
    randr crtc of the output before the output change notification
    is delivered to the clients. Remove RROutputSetCrtc as it is
    not really necessary. All we have to do is set the output's
    crtc on RRCrtcNotify
  * randr12-8d230319040f0a7f72231da2bf5ec97dc3612e21.diff
    Fix the output->crtc initialization in the old randr setup
  * randr12-aec0d06469a2fa7440fdd5ee03dc256a68704e77.diff
    Fix a crash when rotating the screen. Remember output->crtc
    before setting a NULL mode because RRCrtcNotify now sets
    output->crtc to NULL.  Use the saved crtc to set the new mode.
  * randr12-b2dcfbca2441ca8c561f86a78a76ab59ecbb40e4.diff
    RRScanOldConfig cannot use RRFirstOutput before output is
    configured. RRFirstOutput returns the first active output,
    which won't be set until after RRScanOldConfig is finished
    running. Instead, just use the first output (which is the only
    output present with an old driver, after all).
  * randr12-b4193a2eee80895c5641e77488df0e72a73a3d99.diff
    RRScanOldConfig wasn't getting crtcs set correctly. The output
    crtc is set by RRCrtcNotify, which is called at the end of
    RRScanOldConfig. Several uses of output->crtc in this function
    were wrong.
* Thu Aug 23 2007 sndirsch@suse.de
- i810_dri_fix_freeze.diff:
  * fixes freeze after pressing Ctrl-Alt-BS (X.Org Bug #10809)
* Thu Aug 23 2007 sndirsch@suse.de
- xserver-mode-fuzzy-check.diff:
  * Fix for Xserver being more fuzzy about mode validation
  (Bug #270846)
* Sat Aug 18 2007 sndirsch@suse.de
- disable AIGLX by default; without enabled Composite extension
  (still problematic on many drivers) it's rather useless anyway
- updated xorg.conf manual page
* Sat Aug 11 2007 dmueller@suse.de
- fix fileconflict over doc/MAINTAINERS
- build parallel
* Sat Aug  4 2007 sndirsch@suse.de
- updated Mesa source to bugfix release 7.0.1
* Thu Jul 19 2007 sndirsch@suse.de
- xephyr-sig11-fix.diff:
  * long vs. CARD32 mismatch in KeySym definitions between client
    and server code - this patch seems to fix it (and the input
    rework in head fixed it as well in a different way)
    (Bug #235320)
* Sat Jul 14 2007 sndirsch@suse.de
- fixed build on s390(x)
* Tue Jul  3 2007 sndirsch@suse.de
- added X(7) and security(7) manual pages
* Sat Jun 23 2007 sndirsch@suse.de
- updated Mesa source to final release 7.0
* Thu Jun 21 2007 sndirsch@suse.de
- updated Mesa source to release 7.0 RC1
  * Mesa 7.0 is a stable, follow-on release to Mesa 6.5.3. The only
    difference is bug fixes. The major version number bump is due
    to OpenGL 2.1 API support.
* Wed Jun  6 2007 sndirsch@suse.de
- simplified p_default-module-path.diff
* Tue May 22 2007 sndirsch@suse.de
- disabled build of Xprt
- moved Xdmx, Xephyr, Xnest and Xvfb to new subpackage
  xorg-x11-server-extra
* Wed May  2 2007 sndirsch@suse.de
- commit-c09e68c:
  * Paper over a crash at exit during GLX teardown
* Mon Apr 30 2007 sndirsch@suse.de
- updated to Mesa 6.5.3 sources
- obsoletes the following patches:
  * bug-211314_mesa-destroy_buffers.diff
  * bug-211314_mesa-framebuffer-counting.diff
  * bug-211314-patch-1.diff
  * bug-211314-patch-2.diff
  * bug-211314-patch-3.diff
  * bug-211314-patch-4.diff
  * bug-211314-patch-5.diff
  * bug-211314-patch-6.diff
  * bug-211314-patch-7.diff
  * bug-211314-patch-8.diff
  * bug-211314-patch-9.diff
  * bug-211314-patch-10.diff
  * bug-211314-patch-11.diff
  * bug-211314_mesa-refcount-memleak-fixes.diff
  * Mesa-6.5.2-fix_radeon_cliprect.diff
- remove__GLinterface.patch/
  support_mesa6.5.3.patch
  * required Xserver changes for Mesa 6.5.3
* Sat Apr 28 2007 sndirsch@suse.de
- xorg-x11-server-1.2.99-unbreak-domain.patch:
  * This patch fixes some multi-domain systems such as Pegasos with
    xorg-server 1.3. Since pci-rework should get merged soon and
    this patch is a bit of a hack, it never got pushed upstream.
    (X.Org Bug #7248)
* Fri Apr 27 2007 sndirsch@suse.de
- back to Mesa 6.5.2 (Bug #269155/269042)
* Wed Apr 25 2007 sndirsch@suse.de
- Mesa update: 4th RC ready
  * This fixes some breakage in RC3.
* Tue Apr 24 2007 sndirsch@suse.de
- Mesa update: 3rd release candidate
  * updated Windows/VC8 project files.
* Sun Apr 22 2007 sndirsch@suse.de
- updated to Mesa 6.5.3rc2 sources
  * a number of bug fixes since the first RC
* Sat Apr 21 2007 sndirsch@suse.de
- updated to Mesa 6.5.3rc1 sources
- obsoletes the following patches:
  * bug-211314_mesa-destroy_buffers.diff
  * bug-211314_mesa-framebuffer-counting.diff
  * bug-211314-patch-1.diff
  * bug-211314-patch-2.diff
  * bug-211314-patch-3.diff
  * bug-211314-patch-4.diff
  * bug-211314-patch-5.diff
  * bug-211314-patch-6.diff
  * bug-211314-patch-7.diff
  * bug-211314-patch-8.diff
  * bug-211314-patch-9.diff
  * bug-211314-patch-10.diff
  * bug-211314-patch-11.diff
  * bug-211314_mesa-refcount-memleak-fixes.diff
  * Mesa-6.5.2-fix_radeon_cliprect.diff
- GL-Mesa-6.5.3.diff:
  * adjusted GL subdir to Mesa 6.5.3rc1
* Fri Apr 20 2007 sndirsch@suse.de
- xserver 1.3.0.0 release
  * Syncmaster 226 monitor needs 60Hz refresh (#10545).
  * In AIGLX EnterVT processing, invoke driver EnterVT before
    resuming glx.
  * Disable CRTC when SetSingleMode has no matching mode. Update
    RandR as well.
  * Rotate screen size as needed from RandR 1.1 change requests.
  * Add quirk for Acer AL1706 monitor to force 60hz refresh.
  * RandR 1.2 spec says CRTC info contains screen-relative geometry
  * typo in built-in module log message
  * Use default screen monitor for one of the outputs.
  * Allow outputs to be explicitly enabled in config, overriding
    detect.
  * Was accidentally disabling rotation updates in mode set.
  * Disable SourceValidate in rotation to capture cursor.
* Tue Apr 10 2007 sndirsch@suse.de
- Mesa-6.5.2-fix_radeon_cliprect.diff:
  * fixes X.Org Bug #9876
* Fri Apr  6 2007 sndirsch@suse.de
- bug-259290_trapfault.diff:
  * fixes crash caused by bug in XRender code (Bug #259290)
* Fri Apr  6 2007 sndirsch@suse.de
- xserver 1.2.99.905 release:
  * CVE-2007-1003: XC-MISC Extension ProcXCMiscGetXIDList() Memory
    Corruption
  * X.Org Bug #10296: Fix timer rescheduling
- obsoletes bug-243978_xcmisc.diff
* Fri Apr  6 2007 sndirsch@suse.de
- xserver 1.2.99.904 release:
  * Don't erase current crtc for outputs on CloseScreen
* Wed Apr  4 2007 sndirsch@suse.de
- bug-243978_xcmisc.diff:
  * mem corruption in ProcXCMiscGetXIDList (CVE-2007-1003, Bug #243978)
* Wed Apr  4 2007 sndirsch@suse.de
-  bug-211314_mesa-refcount-memleak-fixes.diff:
  * Fix for memleaks and refount bugs (Bug #211314)
* Fri Mar 30 2007 sndirsch@suse.de
- p_default-module-path.diff:
  * only return /usr/%%lib/xorg/modules in "-showDefaultModulePath"
    Xserver option (Bug #257360)
- set Xserver version to 7.2.0 with configure option
  (Bugs #257360, #253702)
* Tue Mar 27 2007 sndirsch@suse.de
- xserver 1.2.99.903 release:
  * Create driver-independent CRTC-based cursor layer.
  * Allow xf86_reload_cursors during server init.
  * Don't wedge when rotating more than one CRTC.
  * Correct ref counting of RRMode structures
  * Remove extra (and wrong) I2C ByteTimeout setting in DDC code.
  * Slow down DDC I2C bus using a RiseFallTime of 20us for old
    monitors.
  * Clean up Rotate state on server reset.
  * Clear allocated RandR screen private structure.
  * Clean up xf86CrtcRec and xf86OutputRec objects at CloseScreen.
  * Make sure RandR events are delivered from RRCrtcSet.
  * Fix Pending property API, adding RRPostPendingProperty.
  * Incorrect extra memory copy in RRChangeOutputProperty.
  * Ensure that crtc desired values track most recent mode.
  * Make pending properties force mode set. And, remove
    AttachScreen calls.
  * Set version to 1.2.99.903 (1.3 RC3)
  * fbdevhw: Consolidate modeset ioctl calling, report failure if
    it modifies mode.
  * fbdevhw: Fix some issues with the previous commit.
  * fbdevhw: Use displayWidth for fbdev virtual width when
    appropriate.
  * fbdevhw: Override RGB offsets and masks after setting initial
    mode.
  * fbdevhw: Consider mode set equal to mode requested if virtual
    width is larger.
  * fbdevhw: Only deal with RGB weight if default visual is True-
    or DirectColor.
  * Add per-drawable Xv colour key helper function.
  * Bump video driver ABI version to 1.2.
* Mon Mar 19 2007 sndirsch@suse.de
- no longer apply bug-211314_mesa-context.diff,
  bug-211314_p_drawable_privclean.diff (Bug #211314, comment #114)
- added different Mesa patches (Bug #211314, comments #114/#115)
* Thu Mar 15 2007 schwab@suse.de
- Remove bug197190-ia64.diff, fix x86emu instead.
* Wed Mar 14 2007 sndirsch@suse.de
- xserver 1.2.99.902 release:
  * Xprint: shorten font filename to fit in tar length limit
  * Move xf86SetSingleMode into X server from intel driver.
  * Add xf86SetDesiredModes to apply desired modes to crtcs.
  * Use EDID data to set screen physical size at server startup.
  * Allow relative positions to use output names or monitor
    identifiers.
  * Add xf86CrtcScreenInit to share initialization across drivers.
  * Add hw/xfree86/docs/README.modes, documenting new mode setting
    APIs.
  * Remove stale monitor data when output becomes disconnected.
  * Revert "Xprint includes a filename which is too long for tar."
  * Revert "Xext: Update device's lastx/lasty when sending a motion
    event with XTest."
  * Xext: Update device's lastx/lasty when sending a motion event
    with XTest.
* Wed Mar 14 2007 sndirsch@suse.de
- xf86crtc_allowdual.diff no longer required; replaced by
  xrandr_12_newmode.diff in xrandr (xorg-x11 package)
* Wed Mar 14 2007 sndirsch@suse.de
- bug197190-ia64.diff:
  * missing -DNO_LONG_LONG for IA64 (Bug #197190)
* Fri Mar  9 2007 sndirsch@suse.de
- xf86crtc_allowdual.diff:
  * allows dualhead even when the second monitor is not yet
    connected during Xserver start
* Tue Mar  6 2007 sndirsch@suse.de
- %%post: replace "i810beta" with "intel" in existing xorg.conf
* Mon Mar  5 2007 sndirsch@suse.de
- xserver 1.2.99.901 release:
  * RandR 1.2
  * EXA damage track
  * minor fixes
* Mon Feb 19 2007 sndirsch@suse.de
- use global permissions files for SUSE > 10.1 (Bug #246228)
* Thu Feb  1 2007 sndirsch@suse.de
- improved bug-197858_dpms.diff to fix Xserver crash (Bug #197858)
* Mon Jan 29 2007 sndirsch@suse.de
- bug-197858_dpms.diff:
  * finally fixed "X server wakes up on any ACPI event" issue
    (Bug #197858)
* Thu Jan 25 2007 sndirsch@suse.de
- bug-211314_p_drawable_privclean.diff:
  * fixed for cleaning up pointers
* Wed Jan 24 2007 sndirsch@suse.de
- fixed build
* Wed Jan 24 2007 sndirsch@suse.de
- bug-211314_p_drawable_privclean.diff:
  * fixes Xserver crash in Mesa software rendering path (Bug #211314)
* Tue Jan 23 2007 sndirsch@suse.de
- xserver 1.2.0 release
  * Bug #9219: Return BadMatch when trying to name the backing
    pixmap of an unrealized window.
  * Bug #9219: Use pWin->viewable instead of pWin->realized to
    catch InputOnly windows too.
  * Fix BSF and BSR instructions in the x86 emulator.
  * Bug #9555: Always define _GNU_SOURCE in glibc environments.
  * Bug #8991: Add glXGetDrawableAttributes dispatch; fix texture
    format therein.
  * Bump video and input ABI minors.
  * Fix release date.
  * Fix syntax error in configure check for SYSV_IPC that broke
    with Sun cc
  * Map missing keycodes for Sun Type 5 keyboard on Solaris SPARC
  * Update pci.ids to 2006-12-06 from pciids.sf.net
  * Xorg & Xserver man page updates for 1.2 release
  * xorg.conf man page should say "XFree86-DGA", not "Xorg-DGA"
  * Xserver man page: remove bc, add -wr
  * Update pci.ids to 2007-01-18 snapshot
  * Update Xserver man page to match commit
    ed33c7c98ad0c542e9e2dd6caa3f84879c21dd61
  * Fix Tooltip from minimized clients
  * Fix Xming fails to use xkb bug
  * Fix bad commit
  * Set Int10Current->Tag for the linux native int10 module
  * added mipmap.c
  * configure.ac: prepare for 1.2.0 (X11R7.2)
  * sparc: don't include asm/kbio.h -- it no longer exists in
    current headers.
  * Minor typos in Xserver man page.
  * Fix several cases where optimized paths were hit when they
    shouldn't be.
  * Try dlsym(RTLD_DEFAULT) first when finding symbols.
  * Fix RENDER issues (bug #7555) and implement RENDER add/remove
    screen
  * For Xvfb, Xnest and Xprt, compile fbcmap.c with -DXFree86Server
  * Multiple integer overflows in dbe and render extensions
  * Require glproto >= 1.4.8 for GLX.
  * __glXDRIscreenProbe: Use drmOpen/CloseOnce.
  * xfree86/hurd: re-add missing keyboard support (bug #5613)
  * remove last remaning 'linux'isms (bug #5613)
- obsoletes
  * Mesa-6.5.2.diff
  * xorg-server-1.1.99.901-GetDrawableAttributes.patch
  * int10-fix.diff
  * cve-2006-6101_6102_6103.diff
- disabled build of VNC server/module
* Wed Jan 17 2007 sndirsch@suse.de
- bug-211314_mesa-context.diff:
  * fixes Xserver crash in software rendering fallback (Bug #211314)
* Tue Jan 16 2007 sndirsch@suse.de
- 0018-vnc-support.txt.diff
  * fixed unresolved symbols vncRandomBytes/deskey in VNC module
    (terminated Xserver when client connected)
* Tue Jan 16 2007 sndirsch@suse.de
- bug227111-ddc_screensize.diff:
  * allow user overrides for monitor settings (Bug #227111)
* Mon Jan 15 2007 sndirsch@suse.de
- loadmod-bug197195.diff:
  * check the complete path (Bug #197195)
* Sun Jan 14 2007 sndirsch@suse.de
- added build of VNC support (0018-vnc-support.txt/
  0018-vnc-support.txt.diff); see 0018-vnc-support.txt.mbox for
  reference
* Tue Jan  9 2007 sndirsch@suse.de
- cve-2006-6101_6102_6103.diff:
  * CVE-2006-6101 iDefense X.org ProcRenderAddGlyphs (Bug #225972)
  * CVE-2006-6102 iDefense X.org ProcDbeGetVisualInfo (Bug #225974)
  * CVE-2006-6103 iDefense X.org ProcDbeSwapBuffers (Bug #225975)
* Tue Dec 19 2006 sndirsch@suse.de
- int10-fix.diff
  * Set Int10Current->Tag for the linux native int10 module (X.Org
    Bug #9296)
  * obsoletes p_initialize-pci-tag.diff
* Tue Dec 19 2006 sndirsch@suse.de
- reverted latest change by schwab (Bug #197190, comment #67)
* Mon Dec 18 2006 schwab@suse.de
- Fix off-by-one in pci multi-domain support [#229278].
* Wed Dec 13 2006 sndirsch@suse.de
- libdrm.diff:
  * no longer fail when some driver tries to load "drm" module
* Tue Dec 12 2006 sndirsch@suse.de
- xorg-server-1.1.99.901-GetDrawableAttributes.patch:
  * hopefully fixes AIGLX issues (X.Org Bug #8991)
* Fri Dec  8 2006 sndirsch@suse.de
- another 64bit warning fix
* Sat Dec  2 2006 sndirsch@suse.de
- X.Org 7.2RC3 release
  * Add a -showDefaultModulePath option.
  * Add a -showDefaultLibPath option.
  * Add DIX_CFLAGS to util builds.
  * Fix release date, and tag 1.1.99.903
  * make X server use system libdrm - this requires libdrm >= 2.3.0
  * DRI: call drmSetServerInfo() before drmOpen().
  * add extern to struct definition
  * fixup configure.ac problems with DRI_SOURCES and LBX_SOURCES
  * bump to 1.1.99.903
  * remove CID support (bug #5553)
  * dri: setup libdrm hooks as early as possible.
  * Bug #8868: Remove drm from SUBDIRS now that the directory is gone.
  * Fix typo before the last commit.
  * Fix GL context destruction with AIGLX.
  * On DragonFLy, default to /dev/sysmouse (just like on FreeBSD).
  * ffs: handle 0 argument (bug #8968)
  * Bug #9023: Only check mice for "mouse" or "void" if identifier
    is != NULL.  Fix potential NULL pointer access in timer code.
- updated Mesa sources to 6.5.2
* Tue Nov 28 2006 sndirsch@suse.de
- xserver-timers.diff:
  * fix null pointer reference in timer code (Bug #223718)
* Mon Nov 20 2006 sndirsch@suse.de
- p_pci-off-by-one.diff:
  * readded off by one fix, which has been dropped by accident
  (Bug #197190)
* Mon Nov 20 2006 sndirsch@suse.de
- acpi_events.diff:
  * distinguish between general and input devices also for APM
    (Bug #197858)
* Tue Nov 14 2006 sndirsch@suse.de
- removed /etc/X11/Xsession.d/92xprint-xpserverlist (Bug #220733)
* Tue Nov 14 2006 sndirsch@suse.de
- mouse-fix.diff:
  * prevent driver from crashing when something different than
    "mouse" or "void" is specified; only check mice for "mouse"
    or "void" if identifier is != NULL. (X.Org Bug #9023)
* Tue Nov 14 2006 sndirsch@suse.de
- X.Org 7.2RC2 release
- adjusted p_enable-altrix.diff, p_pci-domain.diff
- obsoletes p_pci-ia64.diff, xorg-xserver-ia64-int10.diff
  p_pci-legacy-mmap.diff
- Changes in RC2 since RC1
  Aaron Plattner:
    Fix standard VESA modes.
  Adam Jackson:
    Bug #6786: Use separate defines for server's Fixes support level.
    'make dist' fixes.
    Fix distcheck.
    Include a forgotten ia64 header in the distball.  Builds on ia64 now.
    configure.ac bump.
  Alan Coopersmith:
    Make sure xorgcfg files are included even when dist made with
  - -disable-xorgcfg
    Use getisax() instead of asm code to determine available x86 ISA
    extensions on Solaris
    Pre-release message should tell users to check git, not CVS, for updates
    Fix automake error: BUILT_SOURCES was defined multiple times on Solaris
    Bug #1997: AUDIT messages should contain uid for local accesses
    If getpeerucred() is available, include pid & zoneid in audit messages
    too
    Make _POSIX_C_SOURCE hack work with Solaris headers
  Alan Hourihane:
    Small modification to blocking signals when switching modes.
  Bjorn Helgaas:
    Do not map full 0-1MB legacy range
  Bram Verweij:
    xfree86/linux acpi: fix tokenising
  Daniel Stone:
    GetTimeInMillis: spuport monotonic clock
    WaitForSomething: allow time to rewind
    Revert "WaitForSomething: allow time to rewind"
  Revert "GetTimeInMillis: spuport monotonic clock"
  add 'general socket' handler, port ACPI to use it
  WaitForSomething: allow time to rewind
  WaitForSomething: only rewind when delta is more than 250ms
  GetTimeInMillis: spuport monotonic clock
  GetTimeInMillis: simplify monotonic test
    GetTimeInMillis: use correct units for clock_gettime
    os: fix sun extensions test
  Eamon Walsh:
    Bug #8875: Security extension causes Xorg to core dump on server reset
    whitespace adjust
    More work on Bug #8875: revert previous fix and try using client
    argument
    Bug #8937: Extension setup functions not called on server resets
  Egbert Eich:
    Fixing mach64 driver bailing out on ia64
    Make int10 fully domain aware.
  Erik Andren:
    remove XFree86 changelogs (bug #7262)
  Joshua Baergen:
    Create xorg.conf.example (Gentoo bug #138623).
  Laurence Withers:
    CreateColormap: fix return value (bug #7083)
  Matthias Hopf:
    Build with -D_PC on ix86 only.
    Added missing domain stripping in already domain aware code.
    Added linux 2.6 compatible domain aware device scanning code.
    Fixing domain support for ia64
    Add domain support to linuxPciOpenFile().
    Fix device path in altixPCI.c to be domain aware.
    Fix obviously wrong boundary checks + cleanup unused vars.
  Matthieu Herrb:
    kill GNU-make'ism.
    Handle building in a separate objdir
  Michel Dänzer:
    Fix __glXDRIbindTexImage() for 32 bpp on big endian platforms.
    Fix test for Option "IgnoreABI".
  Myron Stowe:
    xfree86: re-enable chipset-specific drivers for Linux/ia64
  Rich Coe:
    CheckConnections: don't close down the server client (bug #7876)
* Thu Nov  9 2006 sndirsch@suse.de
- p_ppc_domain_workaround.diff:
  * ugly workaround for still missing domain support on ppc
    (Bug #202133)
* Sat Nov  4 2006 sndirsch@suse.de
- updated to snapshot of xserver-1.2-branch (soon to be released
  as X.Org 7.2RC2)
  * Make sure xorgcfg files are included even when dist made with
  - -disable-xorgcfg
  * Small modification to blocking signals when switching modes.
  * Use getisax() instead of asm code to determine available x86
    ISA extensions on Solaris
  * Pre-release message should tell users to check git, not CVS,
    for updates
  * Fix __glXDRIbindTexImage() for 32 bpp on big endian platforms.
  * Create xorg.conf.example (Gentoo bug #138623).
  * Fix test for Option "IgnoreABI".
    This option has plenty of potential for wasting the time of bug
    triagers without pretending it's always on.
  * kill GNU-make'ism.
  * Handle building in a separate objdir
  * Fix automake error: BUILT_SOURCES was defined multiple times on Solaris
  * Bug #1997: AUDIT messages should contain uid for local accesses
  * If getpeerucred() is available, include pid & zoneid in audit messages too
* Wed Nov  1 2006 sndirsch@suse.de
- added /etc/modprobe.d/nvidia
* Wed Oct 25 2006 sndirsch@suse.de
- xorg-xserver-ia64-int10.diff:
  * build int10 module with _PC only on %%ix86 (Bug #197190)
* Mon Oct 23 2006 sndirsch@suse.de
- added build of Xephyr; useful for debugging KDE apps (coolo)
* Tue Oct 17 2006 sndirsch@suse.de
- cfb8-undefined.diff:
  * fixes warning for undefined behaviour
* Tue Oct 17 2006 aj@suse.de
- Own /etc/X11/Xsession.d directory.
* Mon Oct 16 2006 aj@suse.de
- Use /etc/X11/Xsession.d.
* Sat Oct 14 2006 sndirsch@suse.de
- updated to X.Org 7.2RC1
* Fri Oct 13 2006 sndirsch@suse.de
- only disable AIGLX by default on SUSE <= 10.1 (Bug #197093)
- no longer fake release version for fglrx driver (Bug #198125)
* Mon Oct  9 2006 sndirsch@suse.de
- glx-align.patch:
  * reenabled -D__GLX_ALIGN64 on affected plaforms (X.Org Bug #8392)
- Fixes to p_pci-domain.diff (Bug #197572)
  * internal domain number of by one (was supposed to be a cleanup,
    but other code dependet on this semantics)
  * fixed another long-standing of-by-1 error
- p_enable-altrix.diff (Bug #197572)
  * This additional patch enables the build of the altrix detection
    routines, which have apparently not been included in Xorg 7.1
    yet. This patch needs a autoreconf -fi after application.
* Mon Sep 18 2006 sndirsch@suse.de
- updated to Mesa 6.5.1
* Wed Sep 13 2006 sndirsch@suse.de
- disable-fbblt-opt.diff:
  * Disable optimization (introduced by ajax) due to a general vesa
    driver crash later in memcpy (Bug #204324)
* Sat Sep  9 2006 sndirsch@suse.de
- removed two source files with imcompatible license from Mesa
  tarball (Bug #204110)
- added a check to specfile to make sure that these will not be
  reintroduced with the next Mesa update again (Bug #204110)
* Fri Sep  1 2006 sndirsch@suse.de
- moved xf86Parser.h,xf86Optrec.h back to /usr/include/xorg, since
  SaX2 build issues have finally been resolved by making use of
  "-iquote /usr/include/xorg -I."
* Thu Aug 31 2006 sndirsch@suse.de
- disable-root-xorg_conf.diff:
  * no longer consider to read /root/xorg.conf
* Tue Aug 29 2006 sndirsch@suse.de
- only require xorg-x11-fonts-core ('fixed' + 'cursor' fonts)
* Mon Aug 28 2006 sndirsch@suse.de
- fake release version for fglrx driver again, since using
  IgnoreABI does not help (the check for the ABI version is in the
  binary-only fglrx driver)
* Sun Aug 27 2006 sndirsch@suse.de
- added Requires: xorg-x11-driver-{input,video} (Bug #202080)
* Fri Aug 25 2006 sndirsch@suse.de
- ignore-abi.diff:
  * adds IgnoreABI option for xorg.conf (same as -ignoreABI)
- remove .la files
- no longer fake release version for fglrx driver; use the new
  IgnoreABI option instead!
* Fri Aug 25 2006 sndirsch@suse.de
- PCI/IA64 Patches (Bug #197572):
  * apply new p_pci-domain.diff (mhopf)
  * apply new p_pci-ce-x.diff (mhopf)
* Thu Aug 24 2006 sndirsch@suse.de
- PCI/IA64 Patches (Bug #197572):
  * removed p_mappciBIOS_complete.diff (already applied upstream)
  * apply p_pci-ia64.diff
  * apply p_pci-legacy-mmap.diff only for IA64 (as before)
  * disabled for now:
  - p_pci-domain.diff: still issues with it
  - p_pci-ce-x.diff: sits on top of p_pci-domain.diff
* Sun Aug 20 2006 sndirsch@suse.de
- added PCI/IA64 patches, but disabled them for now (Bug #197572)
- remove comp. symlinks in /usr/X11R6/bin for openSUSE >= 10.2
* Fri Aug 18 2006 sndirsch@suse.de
- fixed build for s390/s390x, e.g. use configure options
  - -disable-install-libxf86config
  - -disable-aiglx
  - -disable-dri
  - -disable-xorg
- changed os-name to "openSUSE" instead of "Linux" before
- fake release version for fglrx driver :-(
* Thu Aug 17 2006 sndirsch@suse.de
- xinerama-sig11.diff:
  * prevents Xserver Sig11 with broken Xinerama config (Bug #135002)
* Tue Aug 15 2006 sndirsch@suse.de
- moved /usr/%%_lib/pkgconfig/xorg-server.pc to xorg-x11-server
- added pkgconfig to Requires of xorg-x11-server
* Sat Aug 12 2006 sndirsch@suse.de
- disable-aiglx.diff:
  * disabled AIGLX by default (related to Bug #197093); enable it
    with 'Option "AIGLX" "true"' in ServerFlags section of xorg.conf
* Wed Aug  9 2006 sndirsch@suse.de
- enabled build of aiglx
* Wed Aug  9 2006 sndirsch@suse.de
- patch font path also in xorg.conf when set to /usr/lib/X11/fonts/
  or /usr/X11/lib/X11/fonts
* Tue Aug  8 2006 sndirsch@suse.de
- patch xorg.conf in %%post:
  * radeonold/radeon10b driver --> radeon driver
* Mon Aug  7 2006 sndirsch@suse.de
- added "Requires: xorg-x11-fonts" to prevent issues like
  "could not open default font 'fixed'" for any Xserver
* Mon Aug  7 2006 sndirsch@suse.de
- make sure that symlinks
    /usr/bin/X       --> /var/X11R6/bin/X
    /var/X11R6/bin/X --> /usr/bin/Xorg
  are packaged.
- p_xorg_acpi.diff:
  * fixed for archs which don't have HAVE_ACPI defined, e.g. ppc
* Mon Aug  7 2006 sndirsch@suse.de
- p_xf86Mode.diff:
  * removes wrong warning (Bug #139510)
- p_xorg_acpi.diff:
  * reconnect to acpid when acpid has been killed (Bug #148384)
- p_xkills_wrong_client.diff:
  * This patch has unveiled two other problems. One is rather
    serious as there seems to be a non-zero possibility that the
    Xserver closes the wrong connection and this closes the wrong
    client when it looks for stale sockets of clients that have
    disappeared (eich, Bug #150869)
- p_bug159532.diff:
  * X Clients can intentionally or unintenionally crash X11 by
    using composite on depth 4 pixmaps. This patch fixes this.
    (Bug #159532)
- p_xnest-ignore-getimage-errors.diff:
  * ignores the X error on GetImage in Xnest (Bug #174228,
    X.Org Bug #4411)
- p_initialize-pci-tag.diff:
  * initialize PCI tag correctly, which is used by an IA64 specific
    patch (see Bug #147261 for details); fixes Xserver crashes with
    fglrx driver - and possibly other drivers like vesa - during
    initial startup (!), VT switch and startup of second Xserver
    (SLED10 Blocker Bugs #180535, #170991, #158806)
- p_ia64-console.diff:
  * fixes MCA after start of second Xserver (Bug #177011)
* Sat Aug  5 2006 sndirsch@suse.de
- p_mouse_misc.diff:
  * fix X server crashes with synaptics driver (Bug #61702)
- pu_fixes.diff
  * Fixes not yet in the official version
- p_bug96328.diff:
  * fallback mouse device checking
- p_vga-crashfix.diff:
  * fixes vga driver crash (#133989)
- p_xorg_rom_read.diff
  * read rom in big chunks instead of byte-at-a-time (Bug #140811)
- ps_showopts.diff
  * Xserver "-showopts" option to print available driver options
    (Bug #137374)
* Sat Aug  5 2006 sndirsch@suse.de
- add /var/X11R6/bin directory for now (Bug #197188)
* Wed Aug  2 2006 sndirsch@suse.de
- fix setup line
* Mon Jul 31 2006 sndirsch@suse.de
- fixed fatal compiler warnings
* Mon Jul 31 2006 sndirsch@suse.de
- always (and only) patch xorg.conf if necessary
* Mon Jul 31 2006 sndirsch@suse.de
- update to xorg-server release 1.1.99.3
* Fri Jul 28 2006 sndirsch@suse.de
- use "-fno-strict-aliasing"
* Thu Jul 27 2006 sndirsch@suse.de
- use $RPM_OPT_FLAGS
- remove existing /usr/include/X11 symlink in %%pre
* Wed Jul 26 2006 sndirsch@suse.de
- install xf86Parser.h,xf86Optrec.h to /usr/include instead of
  /usr/include/xorg, so it is no longer necessary to specify
  "-I/usr/include/xorg" which resulted in including a wrong
  "shadow.h" (by X.Org) when building SaX2 (strange build error)
* Tue Jul 25 2006 sndirsch@suse.de
- added permissions files
* Tue Jul 25 2006 sndirsch@suse.de
- add compatibility symlink /usr/X11R6/bin/Xorg
* Fri Jul 21 2006 sndirsch@suse.de
- p_ValidatePci.diff:
  * no longer call ValidatePci() to fix i810 driver issues
    (Bug #191987)
* Thu Jul 20 2006 sndirsch@suse.de
- fixed build
* Tue Jun 27 2006 sndirsch@suse.de
- created package
