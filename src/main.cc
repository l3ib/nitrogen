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
//#include "gettext.h"
//#include <inti/i18n.h>
#include "gcs-i18n.h"

void restore_bgs(SetBG* bg_setter)
{
	Util::program_log("entering restore_bgs()");
	bg_setter->restore_bgs();
	while( Gtk::Main::events_pending() )
	   Gtk::Main::iteration();
	Util::program_log("leaving restore_bgs()");
}

int set_bg_once(Config *cfg, SetBG* bg_setter, Glib::ustring path, int head, SetBG::SetMode mode, bool save, Gdk::Color col, bool random)
{
	Util::program_log("entering set_bg_once()");
	Glib::ustring disp;
    Glib::ustring file;

    if (random) {
        if (path.length() > 0) {
            if (Glib::file_test(path, Glib::FILE_TEST_IS_DIR)) {
                path = Util::pick_random_file(path, cfg->get_recurse());
            }
        } else {
            path = Util::pick_random_file(cfg->get_dirs(), cfg->get_recurse());
        }

        if (path.length() == 0) {
            std::cerr << "Could not find an image to set!\n";
            return 1;
        }
    }

    // path must be resolved to a file at this point, make sure it exists
    if (Glib::file_test(path, Glib::FILE_TEST_IS_REGULAR|Glib::FILE_TEST_IS_SYMLINK)) {
        file = path;
    } else {
        std::cerr << "Set must be called with a file!\n";
        return 1;
    }

    // @TODO: test file for an image format supported

    // saving of pixmaps is for interactive only
    bg_setter->disable_pixmap_save();

    disp = bg_setter->make_display_key(head);
    bool shouldSave = bg_setter->set_bg(disp, file, mode, col);

	if (save && shouldSave)
        Config::get_instance()->set_bg(disp, file, mode, col);

	while (Gtk::Main::events_pending())
		Gtk::Main::iteration();

	Util::program_log("leaving set_bg_once()");
    return 0;
}

bool on_window_close_save_pos(GdkEventAny* event, Gtk::Window *window)
{
   	Config *cfg = Config::get_instance();
    int x, y;
    window->get_position(x, y);
    cfg->set_pos(x, y);

    int w, h;
    window->get_size(w, h);
    cfg->set_size(w, h);

    return false;
}

#ifdef USE_INOTIFY
bool poll_inotify(void) {
	Inotify::Watch::poll(0);
	return true;
}
#endif

int main (int argc, char ** argv) {
    int xin_head = -1;
    bool random = false;
    bool set_once = false;
    SetBG::SetMode mode;


    // set up i18n
    bindtextdomain(PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(PACKAGE, "UTF-8");
    textdomain(PACKAGE);

    /* i18n::set_text_domain_dir(PACKAGE, LOCALEDIR);
       i18n::set_text_domain(PACKAGE);*/

    Gtk::Main kit(argc, argv);
    Gtk::IconTheme::get_default()->append_search_path(NITROGEN_DATA_DIR
            G_DIR_SEPARATOR_S "icons");
    if (!Glib::thread_supported()) Glib::thread_init();
    Config *cfg = Config::get_instance();
    ArgParser *parser = Util::create_arg_parser();

    // parse command line
    if ( ! parser->parse(argc, argv) ) {
        std::cerr << _("Error parsing command line") << ": " << parser->get_error() << "\n";
        return -1;
    }

    // if we got a help, display it and exit
    if ( parser->has_argument("help") ) {
        std::cout << parser->help_text() << "\n";
        return 0;
    }

    // factory to make the correct setter, if no force specified on command line
    SetBG* setter;
    if (parser->has_argument("force-setter")) {
        Glib::ustring setter_str = parser->get_value("force-setter");

        if (setter_str == "xwindows")
            setter = new SetBGXWindows();
#ifdef USE_XINERAMA
        else if (setter_str == "xinerama") {
            setter = new SetBGXinerama();

            XineramaScreenInfo *xinerama_info;
            gint xinerama_num_screens;

            Glib::RefPtr<Gdk::Display> dpy = Gdk::DisplayManager::get()->get_default_display();
            xinerama_info = XineramaQueryScreens(GDK_DISPLAY_XDISPLAY(dpy->gobj()), &xinerama_num_screens);
            ((SetBGXinerama*)setter)->set_xinerama_info(xinerama_info, xinerama_num_screens);
        }
#endif /* USE_XINERAMA */
        else if (setter_str == "gnome")
            setter = new SetBGGnome();
        else if (setter_str == "pcmanfm")
            setter = new SetBGPcmanfm();
        else if (setter_str == "xfce")
            setter = new SetBGXFCE();
        else
            setter = SetBG::get_bg_setter();

    } else {
        setter = SetBG::get_bg_setter();
    }

    // if we got restore, set it and exit
    if ( parser->has_argument("restore") ) {
        restore_bgs(setter);
        return 0;
    }

    // get the starting dir
    std::string startdir = parser->get_extra_args();
    bool bcmdlinedir = startdir.length() > 0;
    if (bcmdlinedir) {
        startdir = Util::fix_start_dir(std::string(startdir));

        if (!Glib::file_test(startdir, Glib::FILE_TEST_EXISTS)) {
            std::cerr << "Given path \"" << startdir << "\" does not exist!\n";
            return 1;
        }
    }

    // load configuration if there is one
    cfg->load_cfg();

    // should we set on the command line?
    Gdk::Color color("#000000");
    if ( parser->has_argument("set-color") ) {
        Glib::ustring bgcolor_str = parser->get_value ("set-color");
        color.parse(bgcolor_str);
    }

    if ( parser->has_argument("head") )
        xin_head = parser->get_intvalue("head");

    if (parser->has_argument("random"))
        random = true;

    if ( parser->has_argument("set-tiled") )	{
        set_once = true;
        mode = SetBG::SET_TILE;
    }

    if ( parser->has_argument("set-scaled") )	{
        set_once = true;
        mode = SetBG::SET_SCALE;
    }

    if ( parser->has_argument("set-auto") )	{
        set_once = true;
        mode = SetBG::SET_AUTO;
    }

    if ( parser->has_argument("set-zoom") )	{
        set_once = true;
        mode = SetBG::SET_ZOOM;
    }

    if ( parser->has_argument("set-zoom-fill") )	{
        set_once = true;
        mode = SetBG::SET_ZOOM_FILL;
    }

    if ( parser->has_argument("set-centered") )	{
        set_once = true;
        mode = SetBG::SET_CENTER;
    }

    if (set_once) {
        set_once = true;
        return set_bg_once(cfg, setter, startdir, xin_head, mode, parser->has_argument("save"), color, random);
    }

    if ( parser->has_argument("no-recurse") )
        cfg->set_recurse(false);
    else
    	cfg->set_recurse(cfg->get_recurse());

    guint w, h;
    cfg->get_size(w, h);

    gint x, y;
    cfg->get_pos(x, y);

    // create main window
    NWindow* main_window = new NWindow(setter);
    main_window->set_default_size(w, h);
    main_window->move(x, y);                    // most likely will be ignored by the wm

    //	main_window->view.set_dir(startdir);
    if (bcmdlinedir)
        main_window->view.load_dir(startdir);
    else
        main_window->view.load_dir(cfg->get_dirs());

    main_window->view.set_current_display_mode(cfg->get_display_mode());
    main_window->set_default_selections();
    main_window->view.set_icon_captions(cfg->get_icon_captions());
    main_window->signal_delete_event().connect(sigc::bind(sigc::ptr_fun(&on_window_close_save_pos), main_window));
    main_window->show();

    main_window->set_default_display(xin_head);

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
    else
    	main_window->view.set_sort_mode(cfg->get_sort_mode());

    // remove parser
    delete parser;

    // rig up idle/thread routines
    Glib::Thread::create(sigc::mem_fun(main_window->view, &Thumbview::load_cache_images), true);
    Glib::Thread::create(sigc::mem_fun(main_window->view, &Thumbview::create_cache_images), true);

#ifdef USE_INOTIFY
    Glib::signal_timeout().connect(sigc::ptr_fun(&poll_inotify), (unsigned)1e3);
#endif

    Gtk::Main::run (*main_window);

    cfg->save_cfg();

    return 0;
}
