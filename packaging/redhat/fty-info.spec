#
#    fty-info - Agent which returns rack controller information
#
#    Copyright (C) 2014 - 2017 Eaton                                        
#                                                                           
#    This program is free software; you can redistribute it and/or modify   
#    it under the terms of the GNU General Public License as published by   
#    the Free Software Foundation; either version 2 of the License, or      
#    (at your option) any later version.                                    
#                                                                           
#    This program is distributed in the hope that it will be useful,        
#    but WITHOUT ANY WARRANTY; without even the implied warranty of         
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          
#    GNU General Public License for more details.                           
#                                                                           
#    You should have received a copy of the GNU General Public License along
#    with this program; if not, write to the Free Software Foundation, Inc.,
#    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.            
#

# To build with draft APIs, use "--with drafts" in rpmbuild for local builds or add
#   Macros:
#   %_with_drafts 1
# at the BOTTOM of the OBS prjconf
%bcond_with drafts
%if %{with drafts}
%define DRAFTS yes
%else
%define DRAFTS no
%endif
Name:           fty-info
Version:        1.0.0
Release:        1
Summary:        agent which returns rack controller information
License:        GPL-2.0+
URL:            https://42ity.com/
Source0:        %{name}-%{version}.tar.gz
Group:          System/Libraries
# Note: ghostscript is required by graphviz which is required by
#       asciidoc. On Fedora 24 the ghostscript dependencies cannot
#       be resolved automatically. Thus add working dependency here!
BuildRequires:  ghostscript
BuildRequires:  asciidoc
BuildRequires:  automake
BuildRequires:  autoconf
BuildRequires:  libtool
BuildRequires:  pkgconfig
BuildRequires:  systemd-devel
BuildRequires:  systemd
%{?systemd_requires}
BuildRequires:  xmlto
BuildRequires:  gcc-c++
BuildRequires:  zeromq-devel
BuildRequires:  czmq-devel
BuildRequires:  malamute-devel
BuildRequires:  cxxtools-devel
BuildRequires:  tntdb-devel
BuildRequires:  fty-proto-devel
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
fty-info agent which returns rack controller information.

%package -n libfty_info1
Group:          System/Libraries
Summary:        agent which returns rack controller information shared library

%description -n libfty_info1
This package contains shared library for fty-info: agent which returns rack controller information

%post -n libfty_info1 -p /sbin/ldconfig
%postun -n libfty_info1 -p /sbin/ldconfig

%files -n libfty_info1
%defattr(-,root,root)
%{_libdir}/libfty_info.so.*

%package devel
Summary:        agent which returns rack controller information
Group:          System/Libraries
Requires:       libfty_info1 = %{version}
Requires:       zeromq-devel
Requires:       czmq-devel
Requires:       malamute-devel
Requires:       cxxtools-devel
Requires:       tntdb-devel
Requires:       fty-proto-devel

%description devel
agent which returns rack controller information development tools
This package contains development files for fty-info: agent which returns rack controller information

%files devel
%defattr(-,root,root)
%{_includedir}/*
%{_libdir}/libfty_info.so
%{_libdir}/pkgconfig/libfty_info.pc
%{_mandir}/man3/*
%{_mandir}/man7/*

%prep
%setup -q

%build
sh autogen.sh
%{configure} --enable-drafts=%{DRAFTS} --with-systemd-units
make %{_smp_mflags}

%install
make install DESTDIR=%{buildroot} %{?_smp_mflags}

# remove static libraries
find %{buildroot} -name '*.a' | xargs rm -f
find %{buildroot} -name '*.la' | xargs rm -f

%files
%defattr(-,root,root)
%doc README.md
%{_bindir}/fty-info
%{_mandir}/man1/fty-info*
%config(noreplace) %{_sysconfdir}/fty-info/fty-info.cfg
/usr/lib/systemd/system/fty-info.service
%dir %{_sysconfdir}/fty-info
%if 0%{?suse_version} > 1315
%post
%systemd_post fty-info.service
%preun
%systemd_preun fty-info.service
%postun
%systemd_postun_with_restart fty-info.service
%endif

%changelog
