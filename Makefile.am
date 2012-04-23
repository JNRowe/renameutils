ACLOCAL_AMFLAGS = -I m4 -I gnulib-m4

SUBDIRS = po gnulib-po gnulib-lib src

.PHONY: rpm debian

debian:
	dpkg-buildpackage -rfakeroot -us -uc -sd -b

rpm: @PACKAGE@.spec
	fakeroot rpmbuild --clean -bb @PACKAGE@.spec

EXTRA_DIST = \
  MANIFEST.sources \
  debian/changelog \
  debian/control \
  debian/copyright \
  debian/rules \
  gnulib-m4/gnulib-cache.m4

distclean-local:
	-rm -f build-stamp configure-stamp \
	   debian/files debian/@PACKAGE@.substvars \
	   debian/stamp-*
	-rm -rf debian/@PACKAGE@
