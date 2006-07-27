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

#include "main.h"
#include "NWindow.h"
#include "Config.h"
#include "SetBG.h"
#include "ArgParser.h"
#include <stdio.h>

// parafarmance testing / bug finding
// http://primates.ximian.com/~federico/news-2006-03.html#09
void
program_log (const char *format, ...)
{
	va_list args;
    char *formatted, *str;

    va_start (args, format);
    formatted = g_strdup_vprintf (format, args);
    va_end (args);

    str = g_strdup_printf ("MARK: %s: %s", g_get_prgname(), formatted);
    g_free (formatted);

	access (str, F_OK);
    g_free (str);
} 

// TODO: friggin move these somewhere
// <2k> I concur.
void set_saved_bgs() {

	program_log("entering set_saved_bgs()");
	
	Glib::ustring file, display;
	SetBG::SetMode mode;
	Gdk::Color bgcolor;
	
	std::vector<Glib::ustring> displist;
	std::vector<Glib::ustring>::const_iterator i;
	Config *cfg = Config::get_instance();

	if ( cfg->get_bg_groups(displist) == false ) {
		std::cerr << "Could not get bg groups from config file." << std::endl;
		return;
	}

	for (i=displist.begin(); i!=displist.end(); i++) {
			
		display = (*i);
		program_log("display: %s", display.c_str());
		
		if (cfg->get_bg(display, file, mode, bgcolor)) {
				
			program_log("setting bg on %s to %s (mode: %d)", display.c_str(), file.c_str(), mode);
			SetBG::set_bg(display, file, mode, bgcolor);
			program_log("set bg on %s to %s (mode: %d)", display.c_str(), file.c_str(), mode);
			
		} else {
			std::cerr << "Could not get bg info" << std::endl;
		}
	}

	program_log("leaving set_saved_bgs()");
	
}

void restore_bgs()
{
	program_log("entering restore_bgs()");
	set_saved_bgs();
	while( Gtk::Main::events_pending() )
	   Gtk::Main::iteration();
	program_log("leaving restore_bgs()");
}

// Converts a relative path to an absolute path.
// Probably works best if the path doesn't start with a '/' :)
// XXX: There must be a better way to do this, haha.
Glib::ustring path_to_abs_path(Glib::ustring path) {
	unsigned parents = 0;
	Glib::ustring cwd = Glib::get_current_dir();
	Glib::ustring::size_type pos;

	// find out how far up we have to go.
	while( (pos = path.find("../", 0)) !=Glib::ustring::npos ) {
		path.erase(0, 3);
		parents++;
	}
	
	// now erase that many directories from the path to the current
	// directory.
	
	while( parents ) {
		if ( (pos = cwd.rfind ('/')) != Glib::ustring::npos) {
			// remove this directory from the path
			cwd.erase (pos);
		}
		parents--;
	}
	
	return cwd + '/' + path;
}

int main (int argc, char ** argv) {

	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " [dir]\n";
		exit(1);
	}
	
	NWindow * main_window;
	Gtk::Main kit (argc, argv);
	Glib::thread_init();
	ArgParser parser;
	Config *cfg = Config::get_instance();

	// ARG PARSING
	parser.register_option("restore", "Restore saved backgrounds");
	parser.register_option("no-recurse", "Do not recurse into subdirectories");
	parser.register_option("sort", "How to sort the backgrounds. Valid options are:\n\t\t\t* alpha, for alphanumeric sort\n\t\t\t* ralpha, for reverse alphanumeric sort\n\t\t\t* time, for last modified time sort (oldest first)\n\t\t\t* rtime, for reverse last modified time sort (newest first)", true);

	if ( ! parser.parse(argc, argv) ) {
		std::cerr << "Error parsing command line: " << parser.get_error() << "\n";
		return -1;
	}

	if ( parser.has_argument("restore") ) {
		restore_bgs();
		return 0;
	}

	if ( parser.has_argument("no-recurse") ) 
		cfg->set_recurse(false);

	if ( parser.has_argument("help") ) {
		std::cout << parser.help_text() << "\n";
		return 0;
	}

	std::string startdir(parser.get_extra_args());
	if ( startdir.length() <= 0 ) {
		std::cerr << "Error: no directory specified!\n";
		return -1;
	}

	// trims the terminating '/'. if it exists
	if ( startdir[startdir.length()-1] == '/' ) {
		startdir.resize(startdir.length()-1);
	}
	
	// if this is not an absolute path, make it one.
	if ( !Glib::path_is_absolute(startdir) ) {
		startdir = path_to_abs_path(startdir);
	}
	
	main_window = new NWindow();
	main_window->view.set_dir(startdir);
	main_window->view.load_dir();
	main_window->set_default_selections();
	main_window->show();

	if ( parser.has_argument("sort") ) {
		Glib::ustring sort_mode = parser.get_value ("sort");
		Thumbview::SortMode mode;
		
		if (sort_mode == "alpha") {
			mode = Thumbview::SORT_ALPHA;
		} else if (sort_mode == "ralpha") {
			mode = Thumbview::SORT_RALPHA;
		} else if (sort_mode == "time") {
			mode = Thumbview::SORT_TIME;
		} else {
			mode = Thumbview::SORT_RTIME;
		}
		
		main_window->view.set_sort_mode (mode);
	}

	// rig up idle pice
	Glib::signal_idle().connect(sigc::mem_fun(main_window->view, &Thumbview::make_cache_images));
	
	Gtk::Main::run (*main_window);
	return 0;
}
