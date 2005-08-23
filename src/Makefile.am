SUBDIRS = common editformats

AM_CPPFLAGS = \
  -I$(top_srcdir)/lib \
  -I$(top_builddir)/lib
AM_CFLAGS = -Wall

bin_PROGRAMS = qcmd icmd deurlname
man_MANS = qcmd.1 imv.1 icp.1 deurlname.1
EXTRA_DIST = $(man_MANS)

qcmd_SOURCES = \
  apply.c \
  display.c \
  edit.c \
  list.c \
  interactive.c \
  plan.c \
  qcmd.c \
  qcmd.h \
  quote.c \
  variables.c
qcmd_LDADD = \
  editformats/lib_editformats.a \
  common/lib_common.a \
  ../lib/libgnu.a \
  $(READLINE_LIBS)

icmd_SOURCES = \
  icmd.c
icmd_LDADD = \
  common/lib_common.a \
  ../lib/libgnu.a \
  $(READLINE_LIBS)

deurlname_SOURCES = \
  deurlname.c
deurlname_LDADD = \
  common/lib_common.a \
  ../lib/libgnu.a

all-local:
	@[ -f qmv ] || (echo $(LN_S) qcmd qmv ; $(LN_S) qcmd qmv)
	@[ -f qcp ] || (echo $(LN_S) qcmd qcp ; $(LN_S) qcmd qcp)
	@[ -f imv ] || (echo $(LN_S) icmd imv ; $(LN_S) icmd imv)
	@[ -f icp ] || (echo $(LN_S) icmd icp ; $(LN_S) icmd icp)

install-exec-local:
	@[ -f $(bindir)/qmv ] || (echo $(LN_S) qcmd $(DESTDIR)$(bindir)/qmv ; $(LN_S) qcmd $(DESTDIR)$(bindir)/qmv)
	@[ -f $(bindir)/qcp ] || (echo $(LN_S) qcmd $(DESTDIR)$(bindir)/qcp ; $(LN_S) qcmd $(DESTDIR)$(bindir)/qcp)
	@[ -f $(bindir)/imv ] || (echo $(LN_S) icmd $(DESTDIR)$(bindir)/imv ; $(LN_S) icmd $(DESTDIR)$(bindir)/imv)
	@[ -f $(bindir)/icp ] || (echo $(LN_S) icmd $(DESTDIR)$(bindir)/icp ; $(LN_S) icmd $(DESTDIR)$(bindir)/icp)

install-data-local:
	$(mkdir_p) $(DESTDIR)$(man1dir)
	@[ -f $(man1dir)/qmv.1 ] || (echo $(LN_S) qcmd.1 $(DESTDIR)$(man1dir)/qmv.1 ; $(LN_S) qcmd.1 $(DESTDIR)$(man1dir)/qmv.1)
	@[ -f $(man1dir)/qcp.1 ] || (echo $(LN_S) qcmd.1 $(DESTDIR)$(man1dir)/qcp.1 ; $(LN_S) qcmd.1 $(DESTDIR)$(man1dir)/qcp.1)

clean-local:
	rm -f qmv qcp imv icp qmv.1 qcp.1
