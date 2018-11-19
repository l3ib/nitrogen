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
            UNKNOWN,
            XINERAMA,
            NEMO,
            PCMANFM,
            IGNORE      // Conky, etc
        };

        typedef struct RootWindowData {
            Window window;
            RootWindowType type;
            std::string wm_class;

            RootWindowData() : type(UNKNOWN) {}
            RootWindowData(Window newWindow) : type(UNKNOWN), window(newWindow) {}
            RootWindowData(Window newWindow, RootWindowType newType) : type(newType), window(newWindow) {}
            RootWindowData(Window newWindow, RootWindowType newType, std::string newClass) : type(newType), window(newWindow), wm_class(newClass) {}
        } RootWindowData;

		virtual bool set_bg(Glib::ustring &disp,
                            Glib::ustring file,
                            SetMode mode,
                            Gdk::Color bgcolor);

        virtual void restore_bgs();

        virtual Glib::ustring make_display_key(gint head) = 0;
        virtual std::map<Glib::ustring, Glib::ustring> get_active_displays() = 0;
        virtual Glib::ustring get_fullscreen_key() = 0;

        static SetBG* get_bg_setter();

		static Glib::ustring mode_to_string( const SetMode mode );
		static SetMode string_to_mode( const Glib::ustring str );

        // public methods used on save/shutdown to work with first pixmaps
        void clear_first_pixmaps();
        void reset_first_pixmaps();
        void disable_pixmap_save();

        virtual bool save_to_config();

	protected:

        virtual Glib::ustring get_prefix() = 0;

		static Glib::RefPtr<Gdk::Pixbuf> make_scale(const Glib::RefPtr<Gdk::Pixbuf>, const gint, const gint, Gdk::Color);
		static Glib::RefPtr<Gdk::Pixbuf> make_tile(const Glib::RefPtr<Gdk::Pixbuf>, const gint, const gint, Gdk::Color);
		static Glib::RefPtr<Gdk::Pixbuf> make_center(const Glib::RefPtr<Gdk::Pixbuf>, const gint, const gint, Gdk::Color);
		static Glib::RefPtr<Gdk::Pixbuf> make_zoom(const Glib::RefPtr<Gdk::Pixbuf>, const gint, const gint, Gdk::Color);
		static Glib::RefPtr<Gdk::Pixbuf> make_zoom_fill(const Glib::RefPtr<Gdk::Pixbuf>, const gint, const gint, Gdk::Color);
        static SetMode get_real_mode(const Glib::RefPtr<Gdk::Pixbuf>, const gint, const gint);

		static guint32 GdkColorToUint32(const Gdk::Color);

        static int handle_x_errors(Display *display, XErrorEvent *error);
        static std::vector<RootWindowData> find_desktop_windows(Display *display, Window curwindow);
        static RootWindowData get_root_window_data(Glib::RefPtr<Gdk::Display> display);
        static RootWindowData check_window_type(Display *display, Window window);

        Glib::RefPtr<Gdk::Pixbuf> make_resized_pixbuf(Glib::RefPtr<Gdk::Pixbuf> pixbuf, SetBG::SetMode mode, Gdk::Color bgcolor, gint tarw, gint tarh);
        virtual Glib::RefPtr<Gdk::Display> get_display(const Glib::ustring& disp);

        virtual bool get_target_dimensions(Glib::ustring& disp, gint winx, gint winy, gint winw, gint winh, gint& tarx, gint& tary, gint& tarw, gint& tarh);
        Glib::RefPtr<Gdk::Pixmap> get_or_create_pixmap(Glib::ustring disp, Glib::RefPtr<Gdk::Display> _display, Glib::RefPtr<Gdk::Window> window, gint winw, gint winh, gint wind, Glib::RefPtr<Gdk::Colormap> colormap);


        Pixmap* get_current_pixmap(Glib::RefPtr<Gdk::Display> _display);
        void set_current_pixmap(Glib::RefPtr<Gdk::Display> _display, Pixmap* new_pixmap);

        // data for saving initial pixmap on first set (for restore later)
        bool has_set_once;
        std::map<Glib::ustring, Pixmap*> first_pixmaps;
};

/**
 * Concrete setter for X windows (default).
 */
class SetBGXWindows : public SetBG {
    public:
        virtual std::map<Glib::ustring, Glib::ustring> get_active_displays();
        virtual Glib::ustring get_fullscreen_key();
    protected:
        virtual Glib::ustring get_prefix();
        virtual Glib::ustring make_display_key(gint head);
        virtual Glib::RefPtr<Gdk::Display> get_display(Glib::ustring& disp);
};

#ifdef USE_XINERAMA
class SetBGXinerama : public SetBG {
    public:
        void set_xinerama_info(XineramaScreenInfo* xinerama_info, gint xinerama_num_screens);
        virtual std::map<Glib::ustring, Glib::ustring> get_active_displays();
        virtual Glib::ustring get_fullscreen_key();

    protected:
        // xinerama stuff
        XineramaScreenInfo* xinerama_info;
        gint xinerama_num_screens;

        virtual Glib::ustring get_prefix();
        virtual Glib::ustring make_display_key(gint head);
        virtual bool get_target_dimensions(Glib::ustring& disp, gint winx, gint winy, gint winw, gint winh, gint& tarx, gint& tary, gint& tarw, gint& tarh);
};
#endif

class SetBGGnome : public SetBG {
    public:
		virtual bool set_bg(Glib::ustring &disp,
                            Glib::ustring file,
                            SetMode mode,
                            Gdk::Color bgcolor);

        virtual std::map<Glib::ustring, Glib::ustring> get_active_displays();
        virtual Glib::ustring get_fullscreen_key();
        virtual bool save_to_config();
    protected:
        virtual Glib::ustring get_prefix();
        virtual Glib::ustring make_display_key(gint head);
        virtual Glib::ustring get_gsettings_key();
        virtual void set_show_desktop();
};

class SetBGNemo : public SetBGGnome {
    public:
        virtual bool save_to_config();
    protected:
        virtual Glib::ustring get_gsettings_key();
        virtual void set_show_desktop();
};

class SetBGPcmanfm : public SetBGGnome {
    public:
        virtual Glib::ustring get_fullscreen_key();
		virtual bool set_bg(Glib::ustring &disp,
                            Glib::ustring file,
                            SetMode mode,
                            Gdk::Color bgcolor);

        virtual std::map<Glib::ustring, Glib::ustring> get_active_displays();
        virtual bool save_to_config();
    protected:
        virtual Glib::ustring make_display_key(gint head);
};

class SetBGXFCE : public SetBG {
    public:
        virtual Glib::ustring get_fullscreen_key();
        virtual std::map<Glib::ustring, Glib::ustring> get_active_displays();
        virtual bool save_to_config();
        virtual bool set_bg(Glib::ustring &disp,
                            Glib::ustring file,
                            SetMode mode,
                            Gdk::Color bgcolor);
    protected:
        virtual Glib::ustring get_prefix();
        virtual Glib::ustring make_display_key(gint head);

        bool call_xfconf(Glib::ustring disp, std::string key, const std::vector<std::string>& params);
};

#endif
