SUBDIRS = icons

metainfodir = $(datarootdir)/metainfo
desktopdir = $(datadir)/applications
desktop_DATA = nitrogen.desktop
metainfo_DATA = nitrogen.metainfo.xml

UPDATE_DESKTOP = update-desktop-database $(datadir)/applications || :

install-data-hook: 
	$(UPDATE_DESKTOP)
uninstall-hook: 
	$(UPDATE_DESKTOP)

EXTRA_DIST = icon-theme-installer nitrogen.metainfo.xml

