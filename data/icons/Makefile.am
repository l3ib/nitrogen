theme=hicolor
themedir=$(datadir)/icons/$(theme)
theme_icons = \
	apps,nitrogen-128.png \
	apps,nitrogen-16.png \
	apps,nitrogen-22.png \
	apps,nitrogen-32.png \
	apps,nitrogen-48.png \
	devices,video-display-16.png \
	actions,wallpaper-zoomed-16.png \
	actions,wallpaper-centered-16.png \
	actions,wallpaper-scaled-16.png \
	actions,wallpaper-tiled-16.png \
	mimetypes,image-x-generic-16.png

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
EXTRA_DIST = video-display-16.png wallpaper-zoomed-16.png wallpaper-centered-16.png wallpaper-tiled-16.png nitrogen-22.png nitrogen-32.png nitrogen-48.png wallpaper-scaled-16.png nitrogen-128.png nitrogen-16.png image-x-generic-16.png

