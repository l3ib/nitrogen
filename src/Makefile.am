bin_PROGRAMS = nitrogen
nitrogen_SOURCES = Config.h NWindow.h main.cc md5.h SetBG.cc main.h Thumbview.cc Config.cc NWindow.cc SetBG.h md5.c ArgParser.h ArgParser.cc Thumbview.h ImageCombo.cc ImageCombo.h Inotify.cc Inotify.h Util.h Util.cc NPrefsWindow.cc
LIBS=@NITROGEN_LIBS@
INCLUDES = @NITROGEN_CFLAGS@ \
	-DNITROGEN_DATA_DIR=\"$(pkgdatadir)\"
man_MANS = nitrogen.1
noinst_HEADERS = Config.h NWindow.h SetBG.h main.h md5.h Thumbview.h ImageCombo.h ArgParser.h Inotify.h Util.h gcs-i18n.h NPrefsWindow.h
EXTRA_DIST = $(man_MANS)

AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\"
