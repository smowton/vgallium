# norootforbuild


Name:           xen-tools-vgallium-dom0
ExclusiveArch:  %ix86 x86_64
%define xvers 3.3
%define xvermaj 3
%define changeset 18494
%define xen_build_dir xen-3.3.1-testing
%define with_kmp 0
%define _unpackaged_files_terminate_build 0
BuildRequires:  LibVNCServer-devel SDL-devel automake bin86 curl-devel dev86 graphviz latex2html libjpeg-devel libxml2-devel ncurses-devel openssl openssl-devel pciutils-devel python-devel transfig hermes-devel
%if %suse_version >= 1030
BuildRequires:  texlive texlive-latex
%else
BuildRequires:  te_ams te_latex tetex
%endif
%ifarch x86_64
BuildRequires:  glibc-32bit glibc-devel-32bit
%endif
%if %{?with_kmp}0
BuildRequires:  xorg-x11
%endif
Version:        3.3.1_18494_03
Release:        1.8
License:        GPL v2 only
Group:          System/Kernel
AutoReqProv:    on
Requires:       xen-libs = %{version}
Requires:       bridge-utils multipath-tools python python-curses python-openssl python-pam python-xml pyxml
Conflicts:      xen-tools
Provides:       xen-tools-ioemu = 3.2
Obsoletes:      xen-tools-ioemu <= 3.2
AutoReqProv:    on
PreReq:         %insserv_prereq %fillup_prereq
Summary:        Xen tools for Vgallium dom0
Source0:        xen-3.3.1-testing-src.tar.bz2
Source2:        README.SuSE
Source3:        boot.xen
Source4:        boot.local.xenU
Source5:        init.xend
Source6:        init.xendomains
Source7:        logrotate.conf
Source8:        domUloader.py
Source9:        xmexample.domUloader
Source10:       xmexample.disks
Source11:       block-nbd
Source12:       block-iscsi
Source13:       block-npiv
Source16:       xmclone.sh
Source17:       xend-relocation.sh
Source18:       init.xen_loop
# Xen API remote authentication sources
Source23:       etc_pam.d_xen-api
Source24:       xenapiusers
# sysconfig hook script for Xen
Source25:       xen-updown.sh
# Upstream patches
Patch0:         18406-constify-microcode.patch
Patch1:         18412-x86-page-type-preemptible.patch
Patch2:         18420-x86-page-type-preemptible-fix.patch
Patch3:         18428-poll-single-port.patch
Patch4:         18446-vtd-dom0-passthrough.patch
Patch5:         18456-vtd-dom0-passthrough-cmdline.patch
Patch6:         18464-cpu-hotplug.patch
Patch7:         18468-therm-control-msr.patch
Patch8:         18471-cpu-hotplug.patch
Patch9:         18475-amd-microcode-update.patch
Patch10:        18481-amd-microcode-update-fix.patch
Patch11:        18483-intel-microcode-update.patch
Patch12:        18484-stubdom-ioemu-makefile.patch
Patch13:        18487-microcode-update-irq-context.patch
Patch14:        18488-microcode-free-fix.patch
Patch15:        18509-continue-hypercall-on-cpu.patch
Patch16:        18519-microcode-retval.patch
Patch17:        18520-per-CPU-GDT.patch
Patch18:        18521-per-CPU-TSS.patch
Patch19:        18523-per-CPU-misc.patch
Patch20:        18528-dump-evtchn.patch
Patch21:        18539-pirq-vector-mapping.patch
Patch22:        18547-pirq-vector-mapping-fix.patch
Patch23:        18573-move-pirq-logic.patch
Patch24:        18574-msi-free-vector.patch
Patch25:        18577-bad-assertion.patch
Patch26:        18583-passthrough-locking.patch
Patch27:        18584-evtchn-lock-rename.patch
Patch28:        18620-x86-page-type-preemptible-fix.patch
Patch29:        18637-vmx-set-dr7.patch
Patch30:        18654-xend-vcpus.patch
Patch31:        18656-vtd-alloc-checks.patch
Patch32:        18661-recursive-spinlocks.patch
Patch33:        18720-x86-dom-cleanup.patch
Patch34:        18722-x86-fixmap-reserved.patch
Patch35:        18723-unmap-dom-page-const.patch
Patch36:        18724-i386-highmem-assist.patch
Patch37:        18731-x86-dom-cleanup.patch
Patch38:        18735-x86-dom-cleanup.patch
Patch39:        18741-x86-dom-cleanup-no-hack.patch
Patch40:        18742-x86-partial-page-ref.patch
Patch41:        18745-xend-ioport-irq.patch
Patch42:        18747-x86-partial-page-ref.patch
Patch43:        18771-reduce-GDT-switching.patch
Patch44:        18778-msi-irq-fix.patch
Patch45:        18764-cpu-affinity.patch
Patch46:        18780-cpu-affinity.patch
Patch47:        18799-cpu-affinity.patch
# Will be fixed in 3.3-testing soon
Patch90:        xen-x86-emulate-movnti.patch
# Our patches
Patch100:       xen-config.diff
Patch101:       xend-config.diff
Patch102:       xen-destdir.diff
Patch103:       xen-rpmoptflags.diff
Patch104:       xen-warnings.diff
Patch106:       xen-changeset.diff
Patch107:       xen-paths.diff
Patch108:       xen-xmexample.diff
Patch109:       xen-xmexample-vti.diff
Patch110:       xen-fixme-doc.diff
Patch111:       xen-domUloader.diff
Patch112:       xen-no-dummy-nfs-ip.diff
Patch113:       serial-split.patch
Patch114:       xen-xm-top-needs-root.diff
Patch115:       xen-tightvnc-args.diff
Patch116:       xen-max-free-mem.diff
Patch120:       xen-ioapic-ack-default.diff
Patch121:       xen-lowmem-emergency-pool.diff
Patch122:       block-losetup-retry.diff
Patch123:       block-flags.diff
Patch124:       xen-hvm-default-bridge.diff
Patch125:       xen-hvm-default-pae.diff
Patch126:       xm-test-cleanup.diff
Patch127:       cross-build-fix.diff
Patch130:       tools-xc_kexec.diff
Patch131:       tools-kboot.diff
Patch132:       libxen_permissive.patch
Patch133:       xenapi-console-protocol.patch
Patch134:       xen-disable-qemu-monitor.diff
Patch135:       supported_module.diff
Patch136:       qemu-security-etch1.diff
Patch137:       rpmlint.diff
Patch140:       cdrom-removable.patch
Patch150:       bridge-opensuse.patch
Patch151:       bridge-vlan.diff
Patch152:       bridge-bonding.diff
Patch153:       bridge-hostonly.diff
Patch154:       bridge-record-creation.patch
Patch155:       xend-core-dump-loc.diff
Patch156:       blktap.patch
Patch157:       xen-api-auth.patch
Patch158:       xen-qemu-iscsi-fix.patch
Patch159:       tools-gdbserver-build.diff
Patch160:       network-route.patch
# Needs to go upstream sometime, when python 2.6 is widespread
Patch161:       python2.6-fixes.patch
Patch162:       udev-rules.patch
Patch163:       ioemu-vnc-resize.patch
# Patches for snapshot support
Patch170:       qemu-img-snapshot.patch
Patch171:       ioemu-blktap-fix-open.patch
Patch172:       snapshot-ioemu-save.patch
Patch173:       snapshot-ioemu-restore.patch
Patch174:       snapshot-ioemu-delete.patch
Patch175:       snapshot-xend.patch
Patch180:       ioemu-qcow2-multiblock-aio.patch
Patch181:       ioemu-blktap-image-format.patch
Patch182:       build-tapdisk-ioemu.patch
Patch183:       blktapctrl-default-to-ioemu.patch
Patch184:       ioemu-blktap-barriers.patch
Patch185:       tapdisk-ioemu-logfile.patch
Patch186:       blktap-ioemu-close-fix.patch
Patch187:       ioemu-blktap-zero-size.patch
# Jim's domain lock patch
Patch190:       xend-domain-lock.patch
# Patches from Jan
Patch240:       dump-exec-state.patch
Patch241:       x86-show-page-walk-early.patch
Patch242:       x86-extra-trap-info.patch
Patch243:       x86-alloc-cpu-structs.patch
Patch244:       32on64-extra-mem.patch
Patch245:       msi-enable.patch
# PV Driver Patches
Patch350:       pv-driver-build.patch
Patch351:       xen-ioemu-hvm-pv-support.diff
Patch352:       pvdrv_emulation_control.patch
Patch353:       blktap-pv-cdrom.patch
Patch354:       x86-cpufreq-report.patch
Patch355:       dom-print.patch
# novell_shim patches
Patch400:       hv_tools.patch
Patch401:       hv_xen_base.patch
Patch402:       hv_xen_extension.patch
# Added for OpenTC
Patch403:       otc-xen.diff
Patch999:       tmp_build.patch
Url:            http://www.cl.cam.ac.uk/Research/SRG/netos/xen/
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
%define pysite %(python -c "import distutils.sysconfig; print distutils.sysconfig.get_python_lib()")

%description
Xen tools modified for Vgallium dom0 components; alters Qemu and xend from their ordinary versions.

Authors:
--------
    Ian Pratt <ian.pratt@cl.cam.ac.uk>
    Keir Fraser <Keir.Fraser@cl.cam.ac.uk>
    Christian Limpach <Christian.Limpach@cl.cam.ac.uk>
    Mark Williamson <mark.williamson@cl.cam.ac.uk>
    Ewan Mellor <ewan@xensource.com>
    Christopher Smowton <cs448@cam.ac.uk>

%prep
%setup -q -n %xen_build_dir
%patch0 -p1
%patch1 -p1
%patch2 -p1
%patch3 -p1
%patch4 -p1
%patch5 -p1
%patch6 -p1
%patch7 -p1
%patch8 -p1
%patch9 -p1
%patch10 -p1
%patch11 -p1
%patch12 -p1
%patch13 -p1
%patch14 -p1
%patch15 -p1
%patch16 -p1
%patch17 -p1
%patch18 -p1
%patch19 -p1
%patch20 -p1
%patch21 -p1
%patch22 -p1
%patch23 -p1
%patch24 -p1
%patch25 -p1
%patch26 -p1
%patch27 -p1
%patch28 -p1
%patch29 -p1
%patch30 -p1
%patch31 -p1
%patch32 -p1
%patch33 -p1
%patch34 -p1
%patch35 -p1
%patch36 -p1
%patch37 -p1
%patch38 -p1
%patch39 -p1
%patch40 -p1
%patch41 -p1
%patch42 -p1
%patch43 -p1
%patch44 -p1
%patch45 -p1
%patch46 -p1
%patch47 -p1
%patch90 -p1
%patch100 -p1
%patch101 -p1
%patch102 -p1
%patch103 -p1
%patch104 -p1
%patch106 -p1
%patch107 -p1
%patch108 -p1
%patch109 -p1
%patch110 -p1
%patch111 -p1
%patch112 -p1
%patch113 -p1
%patch114 -p1
%patch115 -p1
%patch116 -p1
%patch120 -p1
%patch121 -p1
%patch122 -p1
%patch123 -p1
%patch124 -p1
%patch125 -p1
%patch126 -p1
#%patch127 -p1
%patch130 -p1
%patch131 -p1
%patch132 -p1
%patch133 -p1
%patch134 -p1
%patch135 -p1
%patch136 -p1
%patch137 -p1
%patch140 -p1
%patch150 -p1
%patch151 -p1
%patch152 -p1
#%patch153 -p1 hostonly
%patch154 -p1
%patch155 -p1
%patch156 -p1
%patch157 -p1
%patch158 -p1
%patch159 -p1
%patch160 -p1
%patch161 -p1
%patch162 -p1
%patch163 -p1
%patch170 -p1
%patch171 -p1
%patch172 -p1
%patch173 -p1
%patch174 -p1
%patch175 -p1
%patch180 -p1
%patch181 -p1
%patch182 -p1
%patch183 -p1
%patch184 -p1
%patch185 -p1
%patch186 -p1
%patch187 -p1
%patch190 -p1
%patch240 -p1
%patch241 -p1
%patch242 -p1
%patch243 -p1
%patch244 -p1
%patch245 -p1
%patch350 -p1
%patch351 -p1
%patch352 -p1
%patch353 -p1
%patch354 -p1
%patch355 -p1
%patch403 -p1
# Don't use shim for now
%ifarch x86_64
%patch400 -p1
%patch401 -p1
%patch402 -p1
%endif
%patch999 -p1

%build
XEN_EXTRAVERSION=%version-%release
XEN_EXTRAVERSION=${XEN_EXTRAVERSION#%{xvers}}
sed -i "s/XEN_EXTRAVERSION[\t ]*.=.*\$/XEN_EXTRAVERSION  = $XEN_EXTRAVERSION/" xen/Makefile
sed -i "s/XEN_CHANGESET[\t ]*=.*\$/XEN_CHANGESET     = %{changeset}/" xen/Makefile
RPM_OPT_FLAGS=${RPM_OPT_FLAGS//-fstack-protector/}
export CFLAGS="${RPM_OPT_FLAGS}"
export RPM_OPT_FLAGS
make -C tools/include/xen-foreign
make tools
cd tools/debugger/gdb
# there are code problems that don't pass the 02-check-gcc-output, hence bitbucket
./gdbbuild 1>/dev/null 2>/dev/null
cd ../../..

%install
test ! -z "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != "/" && rm -rf $RPM_BUILD_ROOT
export CFLAGS="$RPM_OPT_FLAGS" 
export RPM_OPT_FLAGS
make -C tools/include/xen-foreign
# tools
export XEN_PYTHON_NATIVE_INSTALL=1
make -C tools install \
    DESTDIR=$RPM_BUILD_ROOT MANDIR=%{_mandir}
cp tools/debugger/gdb/gdb-6.2.1-linux-i386-xen/gdb/gdbserver/gdbserver-xen $RPM_BUILD_ROOT/usr/bin/gdbserver-xen
make -C tools/misc/serial-split install \
    DESTDIR=$RPM_BUILD_ROOT MANDIR=%{_mandir}
%ifarch x86_64
mkdir -p $RPM_BUILD_ROOT/usr/lib/xen/bin/
ln -s %{_libdir}/xen/bin/qemu-dm $RPM_BUILD_ROOT/usr/lib/xen/bin/qemu-dm
%endif
# docs
make -C docs install \
    DESTDIR=$RPM_BUILD_ROOT MANDIR=%{_mandir} \
    DOCDIR=%{_defaultdocdir}/xen
for name in COPYING %SOURCE2 %SOURCE3 %SOURCE4; do
    install -m 644 $name $RPM_BUILD_ROOT/%{_defaultdocdir}/xen/
done
mkdir -p $RPM_BUILD_ROOT/%{_defaultdocdir}/xen/misc
for name in vtpm.txt crashdb.txt sedf_scheduler_mini-HOWTO.txt; do
    install -m 644 docs/misc/$name $RPM_BUILD_ROOT/%{_defaultdocdir}/xen/misc/
done
# init scripts
mkdir -p $RPM_BUILD_ROOT/etc/init.d
install %SOURCE5 $RPM_BUILD_ROOT/etc/init.d/xend
ln -s /etc/init.d/xend $RPM_BUILD_ROOT/usr/sbin/rcxend
install %SOURCE6 $RPM_BUILD_ROOT/etc/init.d/xendomains
ln -s /etc/init.d/xendomains $RPM_BUILD_ROOT/usr/sbin/rcxendomains
mkdir -p $RPM_BUILD_ROOT/etc/modprobe.d
install -m644 %SOURCE18 $RPM_BUILD_ROOT/etc/modprobe.d/xen_loop
# example config
mkdir -p $RPM_BUILD_ROOT/etc/xen/{vm,examples,scripts}
mv $RPM_BUILD_ROOT/etc/xen/xmexample* $RPM_BUILD_ROOT/etc/xen/examples
rm -f $RPM_BUILD_ROOT/etc/xen/examples/*nbd
install -m644 %SOURCE9 %SOURCE10 $RPM_BUILD_ROOT/etc/xen/examples/
# scripts
rm -f $RPM_BUILD_ROOT/etc/xen/scripts/block-*nbd
install -m755 %SOURCE11 %SOURCE12 %SOURCE13 %SOURCE16 %SOURCE17 $RPM_BUILD_ROOT/etc/xen/scripts/
# Xen API remote authentication files
install -d $RPM_BUILD_ROOT/etc/pam.d
install -m644 %SOURCE23 $RPM_BUILD_ROOT/etc/pam.d/xen-api
install -m644 %SOURCE24 $RPM_BUILD_ROOT/etc/xen/
# sysconfig hook for Xen
mkdir -p $RPM_BUILD_ROOT/etc/sysconfig/network/scripts
mkdir -p $RPM_BUILD_ROOT/etc/sysconfig/network/if-up.d
mkdir -p $RPM_BUILD_ROOT/etc/sysconfig/network/if-down.d
install -m755 %SOURCE25 $RPM_BUILD_ROOT/etc/sysconfig/network/scripts
ln -s /etc/sysconfig/network/scripts/xen-updown.sh $RPM_BUILD_ROOT/etc/sysconfig/network/if-up.d/xen
ln -s /etc/sysconfig/network/scripts/xen-updown.sh $RPM_BUILD_ROOT/etc/sysconfig/network/if-down.d/xen
# logrotate
install -m644 -D %SOURCE7 $RPM_BUILD_ROOT/etc/logrotate.d/xen
# directories
mkdir -p $RPM_BUILD_ROOT/var/lib/xenstored
mkdir -p $RPM_BUILD_ROOT/var/lib/xen/images
mkdir -p $RPM_BUILD_ROOT/var/lib/xen/jobs
mkdir -p $RPM_BUILD_ROOT/var/lib/xen/save
mkdir -p $RPM_BUILD_ROOT/var/lib/xen/dump
mkdir -p $RPM_BUILD_ROOT/var/lib/xen/xend-db/domain
mkdir -p $RPM_BUILD_ROOT/var/lib/xen/xend-db/migrate
mkdir -p $RPM_BUILD_ROOT/var/lib/xen/xend-db/vnet
mkdir -p $RPM_BUILD_ROOT/var/log/xen
mkdir -p $RPM_BUILD_ROOT/var/run/xenstored
ln -s /var/lib/xen/images $RPM_BUILD_ROOT/etc/xen/images
# Bootloader                                                                                                                         
install -m755 %SOURCE8 $RPM_BUILD_ROOT/usr/lib/xen/boot/
# udev support
mkdir -p $RPM_BUILD_ROOT/etc/udev/rules.d
mv $RPM_BUILD_ROOT/etc/udev/rules.d/xen-backend.rules $RPM_BUILD_ROOT/etc/udev/rules.d/40-xen.rules
#%find_lang xen-vm  # po files are misnamed upstream
# Clean up unpackaged files
rm -rf $RPM_BUILD_ROOT/%{_datadir}/doc/xen/qemu/
rm -f  $RPM_BUILD_ROOT/%{_datadir}/doc/qemu/qemu-*
rm -rf $RPM_BUILD_ROOT/%{_defaultdocdir}/xen/ps
rm -rf $RPM_BUILD_ROOT/usr/share/xen/man/man1/qemu/qemu*
rm -f  $RPM_BUILD_ROOT/usr/share/xen/qemu/openbios-sparc32
rm -f  $RPM_BUILD_ROOT/usr/share/xen/qemu/openbios-sparc64
rm -f  $RPM_BUILD_ROOT/usr/sbin/netfix
rm -f  $RPM_BUILD_ROOT/%pysite/*.egg-info
rm -rf $RPM_BUILD_ROOT/html
rm -rf $RPM_BUILD_ROOT/usr/share/doc/xen/README.*
rm -f  $RPM_BUILD_ROOT/%{_libdir}/xen/bin/qemu-dm.debug

%files
%defattr(-,root,root)
#/usr/bin/lomount
/usr/bin/xencons
/usr/bin/xenstore*
/usr/bin/xentrace*
/usr/bin/pygrub
/usr/bin/qemu-img-xen
/usr/bin/tapdisk-ioemu
/usr/bin/gdbserver-xen
/usr/sbin/blktapctrl
/usr/sbin/flask-loadpolicy
/usr/sbin/img2qcow
/usr/sbin/qcow-create
/usr/sbin/qcow2raw
/usr/sbin/rcxend
/usr/sbin/rcxendomains
/usr/sbin/tapdisk
/usr/sbin/xen*
/usr/sbin/xm
/usr/sbin/xsview
/usr/sbin/fs-backend
%dir %{_libdir}/xen
%dir %{_libdir}/xen/bin
%ifarch x86_64
%dir /usr/lib/xen
%dir /usr/lib/xen/bin
%endif
%dir /usr/lib/xen/boot
%{_datadir}/xen/*.dtd
%{_libdir}/xen/bin/readnotes
%{_libdir}/xen/bin/xc_restore
%{_libdir}/xen/bin/xc_save
%{_libdir}/xen/bin/xenconsole
%{_libdir}/xen/bin/xenctx
%{_libdir}/xen/bin/lsevtchn
%{_mandir}/man1/*.1.gz
%{_mandir}/man5/*.5.gz
%{_mandir}/man8/*.8.gz
/var/adm/fillup-templates/*
%dir /var/lib/xen
%dir %attr(700,root,root) /var/lib/xen/images
%dir %attr(700,root,root) /var/lib/xen/save
%dir %attr(700,root,root) /var/lib/xen/dump
%dir /var/lib/xen/xend-db
%dir /var/lib/xen/xend-db/domain
%dir /var/lib/xen/xend-db/migrate
%dir /var/lib/xen/xend-db/vnet
%dir /var/lib/xenstored
%dir /var/log/xen
%dir /var/run/xenstored
/etc/init.d/xend
/etc/init.d/xendomains
%config /etc/logrotate.d/xen
%dir %attr(700,root,root) /etc/xen
/etc/xen/auto
%config /etc/xen/examples
/etc/xen/images
/etc/xen/qemu-ifup
/etc/xen/scripts
/etc/xen/README*
%config /etc/xen/vm
%config /etc/xen/*.sxp
%config /etc/xen/*.xml
%config(noreplace) /etc/xen/xenapiusers
%config /etc/pam.d/xen-api
%config /etc/modprobe.d/xen_loop
%dir /etc/modprobe.d
%dir /etc/udev
%dir /etc/udev/rules.d
/etc/udev/rules.d/40-xen.rules
/etc/sysconfig/network/scripts/xen-updown.sh
/etc/sysconfig/network/if-up.d/xen
/etc/sysconfig/network/if-down.d/xen
%dir %{_defaultdocdir}/xen
%{_defaultdocdir}/xen/COPYING
%{_defaultdocdir}/xen/README.SuSE
%{_defaultdocdir}/xen/boot.local.xenU
%{_defaultdocdir}/xen/boot.xen
%{_defaultdocdir}/xen/misc
%dir %pysite/xen
%dir %pysite/grub
# formerly tools-ioemu
%dir %{_datadir}/xen
%dir %{_datadir}/xen/man
%dir %{_datadir}/xen/man/man1
%dir %{_datadir}/xen/qemu
%dir %{_datadir}/xen/qemu/keymaps
%{_datadir}/xen/qemu/*
%{_datadir}/xen/man/man1/*
%{_libdir}/xen/bin/qemu-dm
%ifarch x86_64
/usr/lib/xen/bin/qemu-dm
/usr/lib64/xen/bin/xc_kexec
%else
/usr/lib/xen/bin/xc_kexec
%endif
/usr/lib/xen/boot/hvmloader
%pysite/xen/*
/usr/lib/xen/boot/domUloader.py
%pysite/grub/*
%pysite/fsimage.so

%clean
#test ! -z "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != "/" && rm -rf $RPM_BUILD_ROOT
#rm -rf $RPM_BUILD_DIR/%xen_build_dir

%post
%{fillup_and_insserv -y -n xend xend}
%{fillup_and_insserv -y -n xendomains xendomains}

%preun
%{stop_on_removal xendomains xend}

%postun
%{restart_on_update xend}
%{insserv_cleanup}
