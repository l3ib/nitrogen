/*

This file is from Nitrogen, an X11 background setter.  
Copyright (C) 2006  Dave Foster & Javeed Shaikh

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#ifndef _SETBG_H_
#define _SETBG_H_

#include "main.h"
#include <gdk/gdkx.h>
#include <math.h>
#ifdef USE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

typedef int (*XErrorHandler)(Display*, XErrorEvent*);

/**
 * Utility class for setting the background image.
 *
 * @author	Dave Foster <daf@minuslab.net>
 * @date	30 Aug 2005
 */
class SetBG {
	public:
        // NOTE: do not change the order of these, they are changed to integer and 
        // put on disk.
		enum SetMode {
			SET_SCALE,
			SET_TILE,
			SET_CENTER,
			SET_ZOOM,
            SET_AUTO,
            SET_ZOOM_FILL
		};

        enum RootWindowType {
            DEFAULT,
            NAUTILUS,
            XFCE,
            UNKNOWN
        };

		virtual bool set_bg(Glib::ustring &disp,
                            Glib::ustring file,
                            SetMode mode,
                            Gdk::Color bgcolor) = 0;

        virtual void restore_bgs();

        virtual Glib::ustring make_display_key(guint head) = 0;

		static SetBG::RootWindowType get_rootwindowtype(Glib::RefPtr<Gdk::Window> rootwin);

        static SetBG* get_bg_setter();

		static Glib::ustring mode_to_string( const SetMode mode );
		static SetMode string_to_mode( const Glib::ustring str );

	protected:

        virtual Glib::ustring get_prefix() = 0;
        virtual Glib::ustring get_fullscreen_key() = 0;
        bool has_prefix(Glib::ustring prefix, Glib::ustring str);

		static Glib::RefPtr<Gdk::Pixbuf> make_scale(const Glib::RefPtr<Gdk::Pixbuf>, const gint, const gint, Gdk::Color);
		static Glib::RefPtr<Gdk::Pixbuf> make_tile(const Glib::RefPtr<Gdk::Pixbuf>, const gint, const gint, Gdk::Color);
		static Glib::RefPtr<Gdk::Pixbuf> make_center(const Glib::RefPtr<Gdk::Pixbuf>, const gint, const gint, Gdk::Color);
		static Glib::RefPtr<Gdk::Pixbuf> make_zoom(const Glib::RefPtr<Gdk::Pixbuf>, const gint, const gint, Gdk::Color);
		static Glib::RefPtr<Gdk::Pixbuf> make_zoom_fill(const Glib::RefPtr<Gdk::Pixbuf>, const gint, const gint, Gdk::Color);
        static SetMode get_real_mode(const Glib::RefPtr<Gdk::Pixbuf>, const gint, const gint);

		static guint32 GdkColorToUint32(const Gdk::Color);
	
        static int handle_x_errors(Display *display, XErrorEvent *error);

};

/**
 * Concrete setter for X windows (default).
 */
class SetBGXWindows : public SetBG {
    public:
		virtual bool set_bg(Glib::ustring &disp,
                            Glib::ustring file,
                            SetMode mode,
                            Gdk::Color bgcolor);

    protected:
        virtual Glib::ustring get_prefix();
        virtual Glib::ustring get_fullscreen_key();
        virtual Glib::ustring make_display_key(guint head);
};

#ifdef USE_XINERAMA
class SetBGXinerama : public SetBG {
    public:
		virtual bool set_bg(Glib::ustring &disp,
                            Glib::ustring file,
                            SetMode mode,
                            Gdk::Color bgcolor);

        void set_xinerama_info(XineramaScreenInfo* xinerama_info, gint xinerama_num_screens);

    protected:
        // xinerama stuff
        XineramaScreenInfo* xinerama_info;
        gint xinerama_num_screens;

        virtual Glib::ustring get_prefix();
        virtual Glib::ustring get_fullscreen_key();
        virtual Glib::ustring make_display_key(guint head);
};
#endif

class SetBGGnome : public SetBG {
    public:
		virtual bool set_bg(Glib::ustring &disp,
                            Glib::ustring file,
                            SetMode mode,
                            Gdk::Color bgcolor);
    protected:
        virtual Glib::ustring get_prefix();
        virtual Glib::ustring get_fullscreen_key();
        virtual Glib::ustring make_display_key(guint head);
};

#endif
