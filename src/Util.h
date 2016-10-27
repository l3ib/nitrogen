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

#ifndef _UTIL_H_
#define _UTIL_H_

#include "ArgParser.h"
#include "SetBG.h"
#include <gtkmm.h>

namespace Util {

	void program_log(const char *format, ...);
	Glib::ustring path_to_abs_path(Glib::ustring path);
	ArgParser* create_arg_parser();
	std::string fix_start_dir(std::string startdir);
    std::string pick_random_file(std::string path, bool recurse = false);
    std::string pick_random_file(VecStrs paths, bool recurse = false);

    std::pair<VecStrs, VecStrs> get_image_files(std::string path, bool recurse = false);
    bool is_image(std::string file);

    bool is_display_relevant(Gtk::Window* window, Glib::ustring display);
    Glib::ustring make_current_set_string(Gtk::Window* window, Glib::ustring filename, Glib::ustring display);

    Glib::ustring color_to_string(Gdk::Color color);
}

#endif

