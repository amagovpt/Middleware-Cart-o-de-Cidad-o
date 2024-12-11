#
# spec file for package pteid-mw
#
# Copyright (c) 2011-2023 Caixa Magica Software
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

%global __brp_check_rpaths %{nil}

#Disable suse-specific checks: there is no way to disable just the lib64 check
%if 0%{?suse_version}
%ifarch x86_64
%define __arch_install_post %{nil}
%endif
%endif

%define git_revision git20230328
%define app_version 3.10.1

Name:           pteid-mw
License:        GPLv2+
Group:          System/Libraries
Version:        3.12.0
Release:        1%{?dist}
Summary:        Portuguese eID middleware
Url:            https://github.com/amagovpt/autenticacao.gov
Vendor:         Portuguese Government
Source0:        https://github.com/amagovpt/autenticacao.gov/archive/v%{version}/autenticacao.gov-%{version}.tar.gz
Patch4:         0004-add-pt.gov.autenticacao.appdata.xml.patch

BuildRequires:  pcsc-lite-devel
BuildRequires:  make
BuildRequires:  swig >= 4.0.0
BuildRequires:  libzip-devel
BuildRequires:  openjpeg2-devel
BuildRequires:  eac-devel

Requires:       pcsc-lite
Requires:       curl
Requires(post): /usr/bin/gtk-update-icon-cache
Requires(postun): /usr/bin/gtk-update-icon-cache
Conflicts:  cartao_de_cidadao

%if 0%{?suse_version}
BuildRequires:  java-11-openjdk-devel
BuildRequires:  libpoppler-qt5-devel
BuildRequires:  libqt5-qtbase-devel
BuildRequires:  libqt5-qttools-devel
BuildRequires:  libqt5-qtdeclarative-devel
BuildRequires:  libqt5-qtquickcontrols2
BuildRequires:  libQt5QuickControls2-devel
BuildRequires:  libQt5Gui-private-headers-devel
BuildRequires:  libxml-security-c-devel
BuildRequires:  libcurl-devel
BuildRequires:  libxerces-c-devel
BuildRequires:  libopenssl-devel
BuildRequires:  update-desktop-files
BuildRequires:  unzip
Requires: pcsc-ccid
Requires: xerces-c
Requires: libqt5-qtquickcontrols
Requires: libqt5-qtgraphicaleffects
%endif

%if 0%{?fedora} || 0%{?rhel}
BuildRequires:  java-11-openjdk-devel
BuildRequires:  qt5-qtbase-devel
BuildRequires:  qt5-qtdeclarative-devel
BuildRequires:  qt5-qtquickcontrols2-devel
BuildRequires:  qt5-qttools-devel
BuildRequires:  libpng-devel
BuildRequires:  cjson-devel
BuildRequires:  xml-security-c-devel
BuildRequires:  poppler-qt5-devel
BuildRequires:  gcc-c++
BuildRequires:  cairo-devel
BuildRequires:  curl-devel
%if 0%{?rhel} == 8
BuildRequires:  openssl3-devel
%else
BuildRequires:  openssl-devel
%endif
BuildRequires:  desktop-file-utils
BuildRequires:  pcsc-lite-ccid
BuildRequires:  xerces-c-devel
Requires:       poppler-qt5
Requires:       pcsc-lite-ccid
Requires:       hicolor-icon-theme
%endif

%description
 The Autenticação.Gov package provides a utility application (eidguiV2), a set of
 libraries and a PKCS#11 module to use the Portuguese Identity Card
 (Cartão de Cidadão) and Chave Móvel Digital in order to authenticate securely
 in certain websites and sign documents.

%package devel
Summary: Development files
%description devel
This package contains the development files.

%prep
%setup -q -n autenticacao.gov-%{version}
%if 0%{?fedora} || 0%{?rhel} >= 8
%endif
%patch -P4 -p1

# move pteid-mw-pt/_src/eidmw/ to root
cd ..
mv autenticacao.gov-%{version} autenticacao.gov-%{version}.tmp
mv autenticacao.gov-%{version}.tmp/pteid-mw-pt/_src/eidmw/ autenticacao.gov-%{version}
cd autenticacao.gov-%{version}
sed -i 's/java-11-openjdk-amd64/java-11-openjdk/' eidlibJava_Wrapper/eidlibJava_Wrapper.pro
sed -i 's/release 8/release 11/' eidlibJava_Wrapper/eidlibJava_Wrapper.pro
# create dirs that git doesn't
#mkdir lib jar
mkdir -p eidlibJava/class

#fix file permissions
find CMD -perm -o=x -type f -exec chmod 644 {} ';'
find applayer -perm -o=x -type f -exec chmod 644 {} ';'
find cardlayer -perm -o=x -type f -exec chmod 644 {} ';'
find misc -perm -o=x -type f -exec chmod 644 {} ';'
find eidguiV2 -perm -o=x -type f -exec chmod 644 {} ';'


%build
%if 0%{?suse_version}
%ifarch x86_64
#./configure --lib+=-L/usr/lib64
qmake-qt5 "PREFIX_DIR += /usr/local" "INCLUDEPATH += /usr/lib64/jvm/java-11-openjdk-11/include/ /usr/lib64/jvm/java-11-openjdk-11/include/linux/" pteid-mw.pro
%else
qmake-qt5 "PREFIX_DIR += /usr/local" "INCLUDEPATH += /usr/lib/jvm/java-11-openjdk-11/include/ /usr/lib/jvm/java-11-openjdk-11/include/linux/" pteid-mw.pro
%endif
%endif

%if 0%{?fedora} || 0%{?rhel}
%{qmake_qt5} PKG_NAME=pteid CONFIG+=release PREFIX_DIR="/usr/local" INCLUDEPATH+="/usr/lib/jvm/java-11-openjdk/include/ /usr/lib/jvm/java-11-openjdk/include/linux/" pteid-mw.pro
#PKG_NAME=pteid CONFIG+=release PREFIX_DIR="/usr/local" INCLUDEPATH+="/usr/lib/jvm/java-11-openjdk/include/ /usr/lib/jvm/java-11-openjdk/include/linux/ %{_includedir}/openssl3 %{_libdir}/openssl3" pteid-mw.pro
%endif

%make_build

%install

#install libs
mkdir -p $RPM_BUILD_ROOT/usr/local/lib/
%make_install INSTALL_ROOT=$RPM_BUILD_ROOT

mkdir -p $RPM_BUILD_ROOT%{_jnidir}/
install -m 755 -p jar/pteidlibj.jar $RPM_BUILD_ROOT%{_jnidir}/

mkdir -p $RPM_BUILD_ROOT/usr/share/applications
install -m 644 debian/pteid-mw-gui.desktop $RPM_BUILD_ROOT/usr/share/applications

mkdir -p $RPM_BUILD_ROOT/usr/share/icons/hicolor/scalable/apps/
install -m 644 -p debian/pteid-scalable.svg $RPM_BUILD_ROOT/usr/share/icons/hicolor/scalable/apps/

mkdir -p $RPM_BUILD_ROOT/usr/share/pixmaps
install -m 644 -p debian/pteid-signature.png $RPM_BUILD_ROOT/usr/share/pixmaps

mkdir -p $RPM_BUILD_ROOT/usr/share/icons/hicolor/64x64/mimetypes/
ln -s -f ../../../../pixmaps/pteid-signature.png $RPM_BUILD_ROOT/usr/share/icons/hicolor/64x64/mimetypes/application-x-signedcc.png
ln -s -f ../../../../pixmaps/pteid-signature.png $RPM_BUILD_ROOT/usr/share/icons/hicolor/64x64/mimetypes/gnome-mime-application-x-signedcc.png

mkdir -p $RPM_BUILD_ROOT/etc/ld.so.conf.d/
echo "/usr/local/lib" > $RPM_BUILD_ROOT/etc/ld.so.conf.d/pteid.conf
%if 0%{?fedora} || 0%{?rhel}
desktop-file-validate %{buildroot}%{_datadir}/applications/pteid-mw-gui.desktop
%endif

%if 0%{?suse_version}
 %suse_update_desktop_file -i pteid-mw-gui Office Presentation
  export NO_BRP_CHECK_RPATH=true
%endif


%post
%if 0%{?fedora} || 0%{?rhel}
# MDV still uses old pcscd services
if [ -x /etc/init.d/pcscd ]
then
  /etc/init.d/pcscd restart
fi

%if 0%{?fedora} >= 16 || 0%{?rhel} >= 8
systemctl restart pcscd.service
%endif
%endif

# suse 11.4 pcscd service seems broken
%if 0%{?suse_version}  > 1140
if [ -x /etc/init.d/pcscd ]
then
  /etc/init.d/pcscd restart
fi
%endif
gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
/sbin/ldconfig > /dev/null 2>&1

%postun
if [ "$1" = "0" ]; then
gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
/sbin/ldconfig > /dev/null 2>&1
fi

%files
/etc/ld.so.conf.d/pteid.conf
/usr/local/lib/*.so.*
/usr/local/bin/eidguiV2
/usr/local/bin/pteiddialogsQTsrv
/usr/local/bin/eidmw_en.qm
/usr/local/bin/eidmw_nl.qm
/usr/local/include/*
/usr/share/applications/*
/usr/share/icons/*
/usr/share/pixmaps/*
/usr/local/share/certs/
/usr/local/share/pteid-mw/www/
/usr/local/share/pteid-mw/fonts/
%{_jnidir}/*

%files devel
/usr/local/include/*
/usr/local/lib/*.so

%changelog
* Sat Jun 01 2024 Sérgio Basto <sergio@serjux.com> - 3.12.0-1
- 3.12.0

* Wed Sep 21 2022 Sérgio Basto <sergio@serjux.com> - 3.8.0-1
- 3.8.0

* Sat Nov 21 2020 Sérgio Basto <sergio@serjux.com> - 3.3.1-2
- rpmlint pteid-mw-3.3.1-1.fc32.src.rpm pteid-mw-3.3.1-1.fc32.x86_64.rpm

* Sat Nov 21 2020 Sérgio Basto <sergio@serjux.com> - 3.3.1-1
- Update pteid-mw to 3.3.1

* Wed Jan 08 2020 Sérgio Basto <sergio@serjux.com> - 3.0.21-3
- Rebuild for Qt 5.13.2

* Mon Dec 02 2019 Sérgio Basto <sergio@serjux.com> - 3.0.21-2
- Better spec file using make install

* Sun Dec 01 2019 Sérgio Basto <sergio@serjux.com> - 3.0.21-1
- 3.0.21

* Fri Nov 29 2019 Sérgio Basto <sergio@serjux.com> - 3.0.20-2
- Enable build for epel 7 and epel 8 build with or higher:
  libzip 1.5.2-1
  poppler 0.66.0-11
  poppler-data 0.4.9-1
  xalan-c 	1.11.0-16
  xerces-c 	3.2.2-3
  xml-security-c 2.0.2-4 (patched)

* Sun Nov 17 2019 Sérgio Basto <sergio@serjux.com> - 3.0.20-1
- 3.0.20

* Tue Apr 16 2019 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  PDF Signature fixes
  Proxy support in SCAP signature

* Mon Feb 11 2019 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New release - version 3.0.16
  SCAP Signature improvements
  Support for new 3072-bit RSA smartcards

* Wed Sep 05 2018 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  PDF signature bugfix and Linux GTK plugin bugfix

* Wed Jul 04 2018 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  Address Change critical bugfix and various minor fixes in SCAP and CMD signature

* Tue Apr 10 2018 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New major version of pteid-mw: first build of 3.0 branch

* Wed Feb 21 2018 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 5339 - Disable checking of the address file hash in SOD only if the card was issued in a specific range of dates

* Thu Feb 15 2018 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 5334 - Disable checking of the address file hash contained and other minor bugfixes

* Fri Oct 27 2017 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 5239 - PKCS#11 corrections and new ECRaizEstado certificate

* Thu Sep 28 2017 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 5176 - Java SDK compatibility work and new CC 004 cert

* Tue Jul 25 2017 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 4973 - Address Change bugfixes and add missing eidlib headers

* Tue Jul 18 2017 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 4962 - Address Change bugfixes and new error messages

* Tue Jul 04 2017 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 4943 - New Production CA Certificates - ECRaizEstado and Multicert Root CA

* Fri Jun 09 2017 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 4823

* Thu Apr 27 2017 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 4672

* Fri Apr 21 2017 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 4662

* Thu Apr 20 2017 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 4654

* Mon Apr 17 2017 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 4633

* Thu Feb 02 2017 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 4397

* Wed Feb 01 2017 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 4391

* Wed Jan 04 2017 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 4309
  Remove libgsoap dependency

* Thu Dec 22 2016 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 4301
  Add new dependency on QT5, drop SCAP hacks


* Wed Oct 19 2016 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 4044
  Dropped bundled xml-security

* Mon Nov 02 2015 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 3867
  Various fixes, new export PDF design and dss-standalone final version

* Tue Dec 09 2014 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 3788
  Add new mimetype for ccsigned files

* Thu Nov 27 2014 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 3781

* Tue Nov 18 2014 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 3771

* Tue Oct 14 2014 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 3715 (Without changes related to the PDF Signature redesign and new features)

* Wed Oct 08 2014 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  Change major version to 2.3.0 and a couple of smallfixes

* Tue Oct 07 2014 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 3714 (Without changes related to the PDF Signature redesign and new features)

* Wed Oct 01 2014 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 3704 (Without changes related to the PDF Signature redesign and new features)

* Mon Sep 29 2014 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 3696 (Without changes related to the PDF Signature redesign and new features)

* Wed Sep 17 2014 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 3577 (plus some later handpicked changes) - New feature: Address Change, Improved XAdES signature and other bugfixes

* Fri Dec 13 2013 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot: revision 3484 - Fixed locking issue on SignPDF

* Mon Dec 09 2013 Andre Guerreiro 
  New SVN snapshot: revision 3468 - Fix CAP Pin change functionality

* Fri Nov 22 2013 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New major version 2.2 revision 3453 - Fixed certificates included in the PDF signature

* Wed Nov 20 2013 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New major version 2.2 revision 3446 - fixed PKCS11 issue for RSA_SHA1 signatures and a couple of small fixes

* Thu Nov 14 2013 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot : revision 3431 - new CA certificates

* Tue Nov 12 2013 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot : revision 3422 - Various bugfixes 

* Wed Sep 11 2013 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot : revision 3400 - GUI Changes as requested by AMA, various bugfixes and new CA Certificates

* Thu Jan 31 2013 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  New SVN snapshot : revision 3271 - Fix in PKCS11 module to support acroread SHA-256 signatures

* Wed Jan 30 2013 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
  Fix %%post and %%postun, it was breaking the library symlinks on upgrade

* Mon Jan 21 2013 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
- New SVN snapshot : revision 3256 - Fix the titlebar in pteidcertinstall.xpi

* Tue Jan 08 2013 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
- Remove references to pteidpp-gempc.so This file is not part of the package anymore

* Fri Dec 07 2012 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
- New SVN snapshot : revision 3153 - Fixes for Adobe Reader interop

* Mon Nov 05 2012 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
- Improved package description

* Wed Oct 31 2012 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
- New SVN snapshot revision 3078 - More fine-grained positioning for signatures and 
  some GUI improvements

* Mon Oct 29 2012 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
- New SVN snapshot revision 3058 - fix multiline issue in signature and parallel build 
  for poppler 

* Fri Oct 26 2012 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
- New major release 2.1 - PDF Signatures is the new feature
 
* Fri Jul 27 2012 Andre Guerreiro <andre.guerreiro@caixamagica.pt>
- Bump to 2957
- Fix Fedora 17 build

