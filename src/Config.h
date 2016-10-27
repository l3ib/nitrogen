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

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <glib.h>
#include "SetBG.h"
#include <vector>
#include "Thumbview.h"

/**
 * Static class that interfaces with the configuration.
 *
 * @author	Dave Foster <daf@minuslab.net>
 * @date	6 Sept 2005
 */
class Config {
	private:

		bool check_dir();

		bool recurse;
        DisplayMode m_display_mode;
        gint m_posx, m_posy;
        guint m_sizex, m_sizey;
        gboolean m_icon_captions;
        VecStrs m_vec_dirs;
	Thumbview::SortMode m_sort_mode;

        std::string get_bg_config_file() const { return get_file("bg-saved.cfg"); }
        std::string get_config_file() const { return get_file("nitrogen.cfg"); }
        std::string get_file(const Glib::ustring filename) const;

	public:

		// instance getter
		static Config* get_instance();

		Config();

        Config* clone();

		// get/set for bgs
		bool get_bg(const Glib::ustring disp, Glib::ustring &file, SetBG::SetMode &mode, Gdk::Color &bgcolor);
		bool set_bg(const Glib::ustring disp, const Glib::ustring file, const SetBG::SetMode mode, Gdk::Color bgcolor);

		// get all groups (bg_saved.cfg)
		bool get_bg_groups(std::vector<Glib::ustring> &groups);

		bool get_recurse() { return recurse; }
		void set_recurse(bool n) { recurse = n; }
        DisplayMode get_display_mode() { return m_display_mode; }
        void set_display_mode(DisplayMode mode) { m_display_mode = mode; }
        bool get_icon_captions() { return m_icon_captions; }
        void set_icon_captions(gboolean caps) { m_icon_captions = caps; }

        void get_pos(gint &x, gint &y);
        void set_pos(gint x, gint y);
        void get_size(guint &w, guint &h);
        void set_size(guint w, guint h);

        void set_sort_mode(Thumbview::SortMode s) { m_sort_mode = s; }
        Thumbview::SortMode get_sort_mode(){ return m_sort_mode; }

        VecStrs get_dirs();
        void set_dirs(const VecStrs& new_dirs);
        bool add_dir(const std::string& dir);
        bool rm_dir(const std::string& dir);

        bool save_cfg();            // save non bg related cfg info
        bool load_cfg();
};

#endif
