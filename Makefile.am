ACLOCAL_AMFLAGS = -I m4

SUBDIRS = po lib src

.PHONY: rpm debian

debian:
	dpkg-buildpackage -rfakeroot -us -uc -sd -b

rpm: @PACKAGE@.spec
	fakeroot rpm --clean -bb @PACKAGE@.spec

EXTRA_DIST = \
  MANIFEST.sources \
  debian/changelog \
  debian/control \
  debian/copyright \
  debian/rules

distclean-local:
	-rm -f build-stamp configure-stamp \
	   debian/files debian/@PACKAGE@.substvars \
	   debian/stamp-*
	-rm -rf debian/@PACKAGE@
