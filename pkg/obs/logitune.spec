Name:           logitune
Version:        0.2.3
Release:        1%{?dist}
Summary:        Logitech device configurator for Linux
License:        GPL-3.0-or-later
URL:            https://github.com/mmaher88/logitune
Source0:        logitune-%{version}.tar.xz

BuildRequires:  cmake >= 3.22
BuildRequires:  gcc-c++

# Qt 6
%if 0%{?suse_version}
BuildRequires:  ninja
BuildRequires:  qt6-base-devel
BuildRequires:  qt6-declarative-devel
BuildRequires:  qt6-svg-devel
BuildRequires:  qt6-shadertools-devel
BuildRequires:  systemd-devel
%else
BuildRequires:  ninja-build
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  qt6-qtsvg-devel
BuildRequires:  qt6-qtshadertools
BuildRequires:  systemd-devel
%endif

%if 0%{?suse_version}
Requires:       qt6-base
Requires:       qt6-declarative
Requires:       qt6-svg
%else
Requires:       qt6-qtbase
Requires:       qt6-qtdeclarative
Requires:       qt6-qtsvg
%endif

%description
Logitune is a Logitech Options+ alternative for Linux. Configure HID++
peripherals like the MX Master 3S: per-app profiles, button remapping,
DPI, SmartShift, scroll, gesture, and thumb wheel settings.

%prep
%autosetup -n logitune-%{version}

%build
# OBS builds from a source tarball with no .git, so feed the version
# through -DLOGITUNE_VERSION; CMakeLists.txt refuses to guess.
%cmake -DBUILD_TESTING=OFF -DLOGITUNE_VERSION=%{version}
%cmake_build

%install
%cmake_install

%post
# Reload udev rules and retag /dev/uinput + hidraw nodes so keystroke
# injection and device access work on first install without a reboot.
# /dev/uinput exists before the package is installed, so uaccess won't
# be applied retroactively unless we trigger a change event.
udevadm control --reload-rules >/dev/null 2>&1 || :
udevadm trigger --subsystem-match=misc --subsystem-match=hidraw --action=change >/dev/null 2>&1 || :

%postun
if [ $1 -eq 0 ] ; then
    udevadm control --reload-rules >/dev/null 2>&1 || :
fi

%files
%{_bindir}/logitune
%{_prefix}/lib/udev/rules.d/71-logitune.rules
%{_datadir}/applications/logitune.desktop
/etc/xdg/autostart/logitune.desktop
%{_datadir}/icons/hicolor/scalable/apps/com.logitune.Logitune.svg
# Device descriptors (JSON + images) and the GNOME shell extension live in
# their own subtrees. Ship the directories so new devices and any
# additional extension resources land automatically.
%{_datadir}/logitune
%{_datadir}/gnome-shell/extensions/logitune-focus@logitune.com
