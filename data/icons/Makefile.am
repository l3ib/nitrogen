theme=hicolor
themedir=$(datadir)/icons/$(theme)
theme_icons = \
	devices,video-display-16.png \
	actions,wallpaper-bestfit-16.png \
	actions,wallpaper-centered-16.png \
	actions,wallpaper-scaled-16.png \
	actions,wallpaper-tiled-16.png

install_icon_exec = $(top_srcdir)/data/icon-theme-installer \
					-t "$(theme)" \
					-s "$(srcdir)" \
					-d "x$(DESTDIR)" \
					-b "$(themedir)" \
					-m "$(mkinstalldirs)" \
					-x "$(INSTALL_DATA)"

install-data-local:
	$(install_icon_exec) -i $(theme_icons)

uninstall-hook:
	$(install_icon_exec) -u $(theme_icons)

MAINTAINERCLEANFILES = Makefile.in
EXTRA_DIST = $(wildcard *.svg *.png)

