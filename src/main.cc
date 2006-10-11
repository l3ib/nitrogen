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

//
// TODO: i turned this kind of into a shitshow with the xinerama, must redo a bit
//
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

	// determine what mode we are going into
	Glib::RefPtr<Gdk::DisplayManager> manager = Gdk::DisplayManager::get();
	Glib::RefPtr<Gdk::Display> disp	= manager->get_default_display();
	
#ifdef USE_XINERAMA
	XineramaScreenInfo* xinerama_info;
	gint xinerama_num_screens;

	int event_base, error_base;
	int xinerama = XineramaQueryExtension(GDK_DISPLAY_XDISPLAY(disp->gobj()), &event_base, &error_base);
	if (xinerama && XineramaIsActive(GDK_DISPLAY_XDISPLAY(disp->gobj()))) {
		xinerama_info = XineramaQueryScreens(GDK_DISPLAY_XDISPLAY(disp->gobj()), &xinerama_num_screens);


		program_log("using xinerama");

		bool foundfull = false;
		for (i=displist.begin(); i!=displist.end(); i++)
			if ((*i) == Glib::ustring("xin_-1"))
				foundfull = true;

		if (foundfull) {
			if (cfg->get_bg(Glib::ustring("xin_-1"), file, mode, bgcolor)) {
				program_log("setting full xinerama screen to %s", file.c_str());
				SetBG::set_bg_xinerama(xinerama_info, xinerama_num_screens, Glib::ustring("xin_-1"), file, mode, bgcolor); 
				program_log("done setting full xinerama screen");
			} else {
				std::cerr << "Could not get bg info for fullscreen xinerama" << std::endl;
			}
		} else {

			// iterate through, pick out the xin_*
			for (i=displist.begin(); i!=displist.end(); i++) {
				if ((*i).find(Glib::ustring("xin_").c_str(), 0, 4) != Glib::ustring::npos) {
					if (cfg->get_bg((*i), file, mode, bgcolor)) {
						program_log("setting %s to %s", (*i).c_str(), file.c_str());
						SetBG::set_bg_xinerama(xinerama_info, xinerama_num_screens, (*i), file, mode, bgcolor);
						program_log("done setting %s", (*i).c_str());
					} else {
						std::cerr << "Could not get bg info for " << (*i) << std::endl;
					}
				}
			}
		}

		// must return here becuase not all systems have xinerama
		program_log("leaving set_saved_bgs()");	
		return;
	} 
#endif

	for (int n=0; n<disp->get_n_screens(); n++) {
				
		display = disp->get_screen(n)->make_display_name();

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

void set_bg_once(Glib::ustring file, SetBG::SetMode mode, bool save)
{
	program_log("entering set_bg_once()");

	// TODO: how to get a default color in here
	Gdk::Color col;

	// TODO: find out why teh config::set_bg will not get me default
	Glib::ustring disp;

	SetBG::set_bg(disp,file,mode,col);
	if (save) Config::get_instance()->set_bg(disp, file, mode, col);
	while (Gtk::Main::events_pending())
		Gtk::Main::iteration();

	program_log("leaving set_bg_once()");
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

#ifdef USE_INOTIFY
bool poll_inotify(void) {
	Inotify::Watch::poll(0);
	return true;
}
#endif

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

// command line set modes
	parser.register_option("set-scaled", "Sets the background to the given file (scaled)");
	parser.register_option("set-tiled", "Sets the background to the given file (tiled)");
	parser.register_option("set-best", "Sets the background to the given file (best-fit)");
	parser.register_option("set-centered", "Sets the background to the given file (centered)");
	parser.register_option("save", "Saves the background permanently");
	
	std::vector<std::string> vecsetopts;
	vecsetopts.push_back("set-scaled");
	vecsetopts.push_back("set-tiled");
	vecsetopts.push_back("set-best");
	vecsetopts.push_back("set-centered");
	parser.make_exclusive(vecsetopts);

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
		std::cerr << "Error: no directory/file specified!\n";
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

	// should we set on the command line?
	if ( parser.has_argument("set-tiled") )	{
		set_bg_once(startdir, SetBG::SET_TILE, parser.has_argument("save"));
		return 0;
	}

	if ( parser.has_argument("set-scaled") )	{
		set_bg_once(startdir, SetBG::SET_SCALE, parser.has_argument("save"));
		return 0;
	}

	if ( parser.has_argument("set-best") )	{
		set_bg_once(startdir, SetBG::SET_BEST, parser.has_argument("save"));
		return 0;
	}

	if ( parser.has_argument("set-centered") )	{
		set_bg_once(startdir, SetBG::SET_CENTER, parser.has_argument("save"));
		return 0;
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
	Glib::signal_idle().connect(sigc::mem_fun(main_window->view, &Thumbview::load_cache_images));

#ifdef USE_INOTIFY
	Glib::signal_timeout().connect(sigc::ptr_fun(&poll_inotify),
		(unsigned)1e3);
#endif
	
	Gtk::Main::run (*main_window);
	return 0;
}
