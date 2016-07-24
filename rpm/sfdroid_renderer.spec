Name:       sfdroid_renderer
Summary:    Renderer for sfdroid
Version:    1.0.0
Release:    1
Group:      System/System Control
License:    LICENSE
URL:        https://github.com/sfdroid/sfdroid_renderer
Source0:    %{name}-%{version}.tar.bz2

BuildRequires:  pkgconfig(glesv1_cm)
BuildRequires:  pkgconfig(egl)
BuildRequires:  pkgconfig(wayland-egl)
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  pkgconfig(sensord-qt5)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(libhardware)
BuildRequires:  droid-hal-devel
BuildRequires:  desktop-file-utils

%description
%{summary}

%prep
%setup -q -n %{name}-%{version}

%build
make

%install
make DESTDIR=%{buildroot} POWERUP=%{powerup} install

desktop-file-install --delete-original \
  --dir %{buildroot}%{_datadir}/applications \
   %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(644,root,root,755)
%attr(755,root,root) %{_bindir}/sfdroid
%attr(755,root,root) %{_bindir}/sfdroid_powerup
%attr(755,root,root) %{_bindir}/am
%attr(755,root,root) %{_bindir}/sfdroid.sh
%config %{_sysconfdir}/dbus-1/system.d/sfdroid.conf
%config %{_sysconfdir}/udev/rules.d/99-sfdroid-uinput.rules
%{_datadir}/applications/*.desktop
%{_datadir}/icons/hicolor/96x96/apps/sfdroid.png
