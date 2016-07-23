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

%description
%{Summary}

%prep
%setup -q -n %{name}-%{version}

%build
make

%install
rm -rf %{buildroot}
make install

%files
%defattr(-,root,root,-)
%{_sbindir}/%{name}
