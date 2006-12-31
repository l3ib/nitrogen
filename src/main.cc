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
#include "Util.h"


void restore_bgs()
{
	Util::program_log("entering restore_bgs()");
	Util::restore_saved_bgs();
	while( Gtk::Main::events_pending() )
	   Gtk::Main::iteration();
	Util::program_log("leaving restore_bgs()");
}

void set_bg_once(Glib::ustring file, SetBG::SetMode mode, bool save)
{
	Util::program_log("entering set_bg_once()");

	// TODO: how to get a default color in here
	Gdk::Color col;

	// TODO: find out why teh config::set_bg will not get me default
	Glib::ustring disp;

	SetBG::set_bg(disp,file,mode,col);
	if (save) Config::get_instance()->set_bg(disp, file, mode, col);
	while (Gtk::Main::events_pending())
		Gtk::Main::iteration();

	Util::program_log("leaving set_bg_once()");
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
	
	Gtk::Main kit(argc, argv);
	Gtk::IconTheme::get_default()->append_search_path(NITROGEN_DATA_DIR G_DIR_SEPARATOR_S "icons");
	Glib::thread_init();
	Config *cfg = Config::get_instance();
	ArgParser *parser = Util::create_arg_parser();

	// parse command line
	if ( ! parser->parse(argc, argv) ) {
		std::cerr << "Error parsing command line: " << parser->get_error() << "\n";
		return -1;
	}

	// if we got restore, set it and exit
	if ( parser->has_argument("restore") ) {
		restore_bgs();
		return 0;
	}

	// if we got a help, display it and exit
	if ( parser->has_argument("help") ) {
		std::cout << parser->help_text() << "\n";
		return 0;
	}

	// get the starting dir
	std::string startdir = parser->get_extra_args();
	if (startdir.length() <= 0) {
		std::cerr << "Must specify a starting directory or background file.\n";
		return 0;
	}
	startdir = Util::fix_start_dir(std::string(startdir));
	
	// should we set on the command line?
	if ( parser->has_argument("set-tiled") )	{
		set_bg_once(startdir, SetBG::SET_TILE, parser->has_argument("save"));
		return 0;
	}

	if ( parser->has_argument("set-scaled") )	{
		set_bg_once(startdir, SetBG::SET_SCALE, parser->has_argument("save"));
		return 0;
	}

	if ( parser->has_argument("set-best") )	{
		set_bg_once(startdir, SetBG::SET_BEST, parser->has_argument("save"));
		return 0;
	}

	if ( parser->has_argument("set-centered") )	{
		set_bg_once(startdir, SetBG::SET_CENTER, parser->has_argument("save"));
		return 0;
	}

	if ( parser->has_argument("no-recurse") ) 
		cfg->set_recurse(false);
	
	NWindow* main_window = new NWindow();
	main_window->view.set_dir(startdir);
	main_window->view.load_dir();
	main_window->set_default_selections();
	main_window->show();

	if ( parser->has_argument("sort") ) {
		Glib::ustring sort_mode = parser->get_value ("sort");
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
	
	// remove parser
	delete parser;

	// rig up idle pice
	Glib::signal_idle().connect(sigc::mem_fun(main_window->view, &Thumbview::load_cache_images));

#ifdef USE_INOTIFY
	Glib::signal_timeout().connect(sigc::ptr_fun(&poll_inotify), (unsigned)1e3);
#endif
	
	Gtk::Main::run (*main_window);
	return 0;
}
