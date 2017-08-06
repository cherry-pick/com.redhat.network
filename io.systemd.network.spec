%define build_date %(date +"%%a %%b %%d %%Y")
%define build_timestamp %(date +"%%Y%%m%%d.%%H%M%%S")

Name:           io.systemd.network
Version:        1
Release:        %{build_timestamp}%{?dist}
Summary:        Systemd Network Interface
License:        ASL2.0
URL:            https://github.com/varlink/io.systemd.network
Source0:        https://github.com/varlink/io.systemd.network/archive/v%{version}.tar.gz
BuildRequires:  autoconf automake pkgconfig
BuildRequires:  libvarlink-devel
BuildRequires:  libnl3-devel

%description
Service to manage network interfaces.

%prep
%setup -q

%build
./autogen.sh
%configure
make %{?_smp_mflags}

%install
%make_install

%files
%license AUTHORS
%license COPYRIGHT
%license LICENSE
%{_bindir}/io.systemd.network

%changelog
* %{build_date} <info@varlink.org> %{version}-%{build_timestamp}
- %{name} %{version}
