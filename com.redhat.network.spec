Name:           com.redhat.network
Version:        1
Release:        1%{?dist}
Summary:        Systemd Network Interface
License:        ASL2.0
URL:            https://github.com/varlink/%{name}
Source0:        https://github.com/varlink/%{name}/archive/%{name}-%{version}.tar.gz
BuildRequires:  meson
BuildRequires:  gcc
BuildRequires:  pkgconfig
BuildRequires:  libvarlink-devel
BuildRequires:  libnl3-devel

%description
Service to manage network interfaces.

%prep
%setup -q

%build
%meson
%meson_build

%check
export LC_CTYPE=C.utf8
%meson_test

%install
%meson_install

%files
%license LICENSE
%{_bindir}/com.redhat.network

%changelog
* Tue Aug 29 2017 <info@varlink.org> 1-1
- com.redhat.network 1
