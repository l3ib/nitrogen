SUBDIRS = icons

appdatadir = $(datarootdir)/appdata
desktopdir = $(datadir)/applications
desktop_DATA = nitrogen.desktop
appdata_DATA = nitrogen.appdata.xml

UPDATE_DESKTOP = update-desktop-database $(datadir)/applications || :

install-data-hook: 
	$(UPDATE_DESKTOP)
uninstall-hook: 
	$(UPDATE_DESKTOP)

EXTRA_DIST = icon-theme-installer nitrogen.appdata.xml

