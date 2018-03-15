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

#include "NWindow.h"
#include "Config.h"
#include <sys/types.h>
#include <sys/wait.h>
#include "gcs-i18n.h"
#include "Util.h"
#include <algorithm>
#include "NPrefsWindow.h"

#ifdef USE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

// leethax constructor

NWindow::NWindow(SetBG* bg_setter) : apply (Gtk::Stock::APPLY), btn_prefs(Gtk::Stock::PREFERENCES) {
    this->bg_setter = bg_setter;

	set_border_width (5);
	set_default_size (450, 500);

	main_vbox.set_spacing (5);
	add (main_vbox);

	// setup imagecombos
	this->setup_select_boxes();

	// bottom hbox
	bot_hbox.set_spacing (5);
	bot_hbox.pack_start (select_mode, FALSE, FALSE, 0);
	bot_hbox.pack_start (select_display, FALSE, FALSE, 0);
	bot_hbox.pack_start(button_bgcolor, FALSE, FALSE, 0);

	bot_hbox.pack_end(apply, FALSE, FALSE, 0);
	bot_hbox.pack_end(btn_prefs, FALSE, FALSE, 0);

	// add to main box
	main_vbox.pack_start (view, TRUE, TRUE, 0);
	main_vbox.pack_start (bot_hbox, FALSE, FALSE, 0);

	// signals
    view.signal_selected.connect(sigc::mem_fun(*this, &NWindow::sighandle_dblclick_item));
	apply.signal_clicked ().connect (sigc::mem_fun(*this, &NWindow::sighandle_click_apply));
    btn_prefs.signal_clicked().connect(sigc::mem_fun(*this, &NWindow::sighandle_btn_prefs));

    // set icon
	try {
		Glib::RefPtr<Gtk::IconTheme> icontheme = Gtk::IconTheme::get_default();
		std::vector<Glib::RefPtr<Gdk::Pixbuf> > vec;
		vec.push_back(icontheme->load_icon("nitrogen", 16, Gtk::ICON_LOOKUP_NO_SVG));
		vec.push_back(icontheme->load_icon("nitrogen", 22, Gtk::ICON_LOOKUP_NO_SVG));
		vec.push_back(icontheme->load_icon("nitrogen", 32, Gtk::ICON_LOOKUP_NO_SVG));
		vec.push_back(icontheme->load_icon("nitrogen", 48, Gtk::ICON_LOOKUP_NO_SVG));
		vec.push_back(icontheme->load_icon("nitrogen", 128, Gtk::ICON_LOOKUP_NO_SVG));
		Glib::ListHandle<Glib::RefPtr<Gdk::Pixbuf> > lister(vec);

		this->set_icon_list(lister);
	} catch (const Gtk::IconThemeError& e) {
		// don't even worry about it!
	}

	// accel group for keyboard shortcuts
	// unfortunately we have to basically make a menu which we never add to the UI
	m_action_group = Gtk::ActionGroup::create();
	m_action_group->add(Gtk::Action::create("FileMenu", ""));
	m_action_group->add(Gtk::Action::create("Quit", Gtk::Stock::QUIT),
						Gtk::AccelKey("<control>Q"),
						sigc::mem_fun(*this, &NWindow::sighandle_accel_quit));

	m_action_group->add(Gtk::Action::create("Close", Gtk::Stock::CLOSE),
						Gtk::AccelKey("<control>W"),
						sigc::mem_fun(*this, &NWindow::sighandle_accel_quit));

    m_action_group->add(Gtk::Action::create("Random", Gtk::Stock::MEDIA_NEXT),
                        Gtk::AccelKey("<control>R"),
                        sigc::mem_fun(*this, &NWindow::sighandle_random));

	m_ui_manager = Gtk::UIManager::create();
	m_ui_manager->insert_action_group(m_action_group);

	add_accel_group(m_ui_manager->get_accel_group());

	Glib::ustring ui = "<ui>"
						"<menubar name='MenuBar'>"
						"<menu action='FileMenu'>"
						"<menuitem action='Random' />"
						"<menuitem action='Close' />"
						"<menuitem action='Quit' />"
						"</menu>"
						"</menubar>"
						"</ui>";
	m_ui_manager->add_ui_from_string(ui);

    m_dirty = false;
}

// shows all of our widgets

void NWindow::show (void) {
    view.show();
    select_mode.show();
    // show only if > 1 entry in box
    if (this->map_displays.size() > 1) select_display.show();
    apply.show();
    bot_hbox.show();
    main_vbox.show();
    button_bgcolor.show();
    btn_prefs.show();

    this->set_title("Nitrogen");
}

/**
 * Handles the user pressing Ctrl+Q or Ctrl+W, standard quit buttons.
 */
void NWindow::sighandle_accel_quit() {
    if (!handle_exit_request())
        hide();
}

/**
 * Handles the user pressing Ctrl+R to get a random image.
 */
void NWindow::sighandle_random() {
    Glib::Rand rand;

    guint size = view.store->children().size();
    guint idx = rand.get_int_range(0, size);

    Glib::ustring tidx = Glib::ustring::compose("%1", idx);
    Gtk::TreeModel::Path path(tidx);
    Gtk::TreeModel::const_iterator iter = view.store->get_iter(path);

    view.set_selected(path, &iter);
}

/**
 * Handles the user double clicking on a row in the view. 
 */
void NWindow::sighandle_dblclick_item (const Gtk::TreeModel::Path& path) {

	// find out which image was double clicked
	Gtk::TreeModel::iterator iter = (view.store)->get_iter(path);
	Gtk::TreeModel::Row row = *iter;

    // preview - set dirty flag, if setter says we should
	m_dirty = this->set_bg(row[view.record.Filename]);
}

/**
 * Handles the user pressing the apply button.
 */
void NWindow::sighandle_click_apply (void) {
    this->apply_bg();
}

/**
 * Performs the apply action
 * Grabs the selected items and
 * calls set_bg on it. It also saves the bg and closes the application if
 * the app is not multiheaded, or the full xin desktop is selected.
 */
void NWindow::apply_bg () {

    // find out which image is currently selected
    Gtk::TreeModel::iterator iter = view.get_selected();
    if (!iter)
        return;

    Gtk::TreeModel::Row row = *iter;
    Glib::ustring file = row[view.record.Filename];
    bool saveToConfig = this->set_bg(file);

    // apply - remove dirty flag
    m_dirty = false;

    SetBG::SetMode mode = SetBG::string_to_mode( this->select_mode.get_active_data() );
    Glib::ustring thedisp = this->select_display.get_active_data();
    Gdk::Color bgcolor = this->button_bgcolor.get_color();

    // save
    if (saveToConfig)
        Config::get_instance()->set_bg(thedisp, file, mode, bgcolor);

    // tell the bg setter to forget about the first pixmap
    bg_setter->clear_first_pixmaps();

    // tell the row that he's now on thedisp
    row[view.record.CurBGOnDisp] = thedisp;

    // reload config "cache"
    view.load_map_setbgs();

    // find items removed by this config set. typically 0 or 1 items, which is the
    // old background on thedisp, but could be 2 items if xinerama individual bgs
    // are replaced by the fullscreen xinerama.
    for (Gtk::TreeIter i = view.store->children().begin(); i != view.store->children().end(); i++)
    {
        Glib::ustring curbgondisp = (*i)[view.record.CurBGOnDisp];
        if (curbgondisp == "")
            continue;

        std::map<Glib::ustring, Glib::ustring>::iterator mapiter = view.map_setbgs.find(curbgondisp);

        // checking for fullscreen xinerama with several screens end
        if( mapiter == view.map_setbgs.end() )
            break;

        // if filenames don't match, this row must be blanked out!
        Glib::ustring filename = (*i)[view.record.Filename];
        if (filename != (*mapiter).second)
        {
            (*i)[view.record.CurBGOnDisp] = "";
            (*i)[view.record.Description] = Glib::ustring(filename, filename.rfind("/")+1);
        }
        else
            (*i)[view.record.Description] = Util::make_current_set_string(this, filename, (*mapiter).first);
    }
}

/**
 * Common handler for window delete or key accels.
 *
 * Prompts the user to save if necessary.
 */
bool NWindow::handle_exit_request()
{
    if (m_dirty)
    {
        int result;
        Gtk::MessageDialog dialog(*this,
            _("You previewed an image without applying it, apply?"), false,
            Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_NONE, true);

        dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
        dialog.add_button(Gtk::Stock::NO, Gtk::RESPONSE_NO);
        dialog.add_button(Gtk::Stock::YES, Gtk::RESPONSE_YES);

        dialog.set_default_response(Gtk::RESPONSE_YES);
        result = dialog.run();

        switch (result)
        {
            case Gtk::RESPONSE_YES:
                this->apply_bg();
                break;
            case Gtk::RESPONSE_NO:
                bg_setter->reset_first_pixmaps();
                break;
            case Gtk::RESPONSE_CANCEL:
            case Gtk::RESPONSE_DELETE_EVENT:
                return true;
        };
    }

    return false;
}

bool NWindow::on_delete_event(GdkEventAny *event)
{
    return handle_exit_request();
}

/**
 * Queries the necessary widgets to get the data needed to set a bg.  *
 * @param	file	The file to set the bg to
 *
 * @returns If the dirty flag should be set or not
 */
bool NWindow::set_bg(const Glib::ustring file) {

	// get the data from the active items
	SetBG::SetMode mode   = SetBG::string_to_mode(this->select_mode.get_active_data());
	Glib::ustring thedisp = this->select_display.get_active_data();
	Gdk::Color bgcolor    = this->button_bgcolor.get_color();

	// set it
    bg_setter->set_bg(thedisp, file, mode, bgcolor);

    return bg_setter->save_to_config();
}

// leethax destructor
NWindow::~NWindow () {}

/**
 * Creates our ImageCombo boxes
 */
void NWindow::setup_select_boxes() {

	Glib::RefPtr<Gtk::IconTheme> icontheme = Gtk::IconTheme::get_default();
	Glib::RefPtr<Gdk::Pixbuf> icon, genericicon, video_display_icon;

	try {
		genericicon = icontheme->load_icon("image-x-generic", 16, Gtk::ICON_LOOKUP_NO_SVG);
	} catch (const Gtk::IconThemeError& e) {
		genericicon = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, 16, 16);
	}

	try {
		video_display_icon = icontheme->load_icon("video-display", 16, Gtk::ICON_LOOKUP_NO_SVG);
	} catch (const Gtk::IconThemeError& e) {
		video_display_icon = genericicon;
	}

	// modes
    icon = genericicon;
    this->select_mode.add_image_row( icon, _("Automatic"), SetBG::mode_to_string(SetBG::SET_AUTO), true );
	try {
		icon = icontheme->load_icon("wallpaper-scaled", 16, Gtk::ICON_LOOKUP_NO_SVG);
		if (!icon) icon = genericicon;
	} catch (const Gtk::IconThemeError& e) {
		icon = genericicon;
	}
	this->select_mode.add_image_row( icon, _("Scaled"), SetBG::mode_to_string(SetBG::SET_SCALE), false );

	try {
		icon = icontheme->load_icon("wallpaper-centered", 16, Gtk::ICON_LOOKUP_NO_SVG);
		if (!icon) icon = genericicon;
	} catch (const Gtk::IconThemeError& e) {
		icon = genericicon;
	}
	this->select_mode.add_image_row( icon, _("Centered"), SetBG::mode_to_string(SetBG::SET_CENTER), false );

	try {
		icon = icontheme->load_icon("wallpaper-tiled", 16, Gtk::ICON_LOOKUP_NO_SVG);
		if (!icon) icon = genericicon;
	} catch (const Gtk::IconThemeError& e) {
		icon = genericicon;
	}
	this->select_mode.add_image_row( icon, _("Tiled"), SetBG::mode_to_string(SetBG::SET_TILE), false );

	try {
		icon = icontheme->load_icon("wallpaper-zoomed", 16, Gtk::ICON_LOOKUP_NO_SVG);
		if (!icon) icon = genericicon;
	} catch (const Gtk::IconThemeError& e) {
		icon = genericicon;
	}

	this->select_mode.add_image_row( icon, _("Zoomed"), SetBG::mode_to_string(SetBG::SET_ZOOM), false );
	this->select_mode.add_image_row( icon, _("Zoomed Fill"), SetBG::mode_to_string(SetBG::SET_ZOOM_FILL), false );


    // @TODO GET ALL THIS INTEL FROM THE SETTER
    //
	// displays
    map_displays = this->bg_setter->get_active_displays();

    for (std::map<Glib::ustring, Glib::ustring>::const_iterator i = map_displays.begin(); i != map_displays.end(); i++) {
        this->select_display.add_image_row( video_display_icon, (*i).second, (*i).first, false);
    }

	return;
}

/**
 * Sets the file selection, mode combo box, and color button to appropriate default values, based on
 * what is in the configuration file.
 */
void NWindow::set_default_selections()
{
    // grab the current config (if there is one)
    Config *cfg = Config::get_instance();
    std::vector<Glib::ustring> cfg_displays;
    if(cfg->get_bg_groups(cfg_displays))
    {
        SetBG::SetMode m;
        Gdk::Color c;
        Glib::ustring default_selection;
        Glib::ustring file;

        if (find(cfg_displays.begin(), cfg_displays.end(), this->bg_setter->get_fullscreen_key()) != cfg_displays.end())
            default_selection = this->bg_setter->get_fullscreen_key();
        else {
            // iterate the displays we know until we find the first one set
            std::map<Glib::ustring, Glib::ustring> known_disps = this->bg_setter->get_active_displays();
            for (std::map<Glib::ustring, Glib::ustring>::const_iterator i = known_disps.begin(); i != known_disps.end(); i++)
            {
                if (find(cfg_displays.begin(), cfg_displays.end(), (*i).first) != cfg_displays.end())
                {
                    default_selection = (*i).first;
                    break;
                }
            }
        }

        // make sure whatever we came up with is in the config file, if not, just return
        if (find(cfg_displays.begin(), cfg_displays.end(), default_selection) == cfg_displays.end())
            return;

        if (!cfg->get_bg(default_selection, file, m, c)) {
            // failed. return?
            return;
        }

        // set em!
        button_bgcolor.set_color(c);
        select_mode.select_value( SetBG::mode_to_string(m) );

        // iterate through filename list
        for (Gtk::TreeIter iter = view.store->children().begin(); iter != view.store->children().end(); iter++)
        {
            if ( (*iter)[view.record.CurBGOnDisp] == default_selection) {
                Glib::signal_timeout().connect(sigc::bind(sigc::mem_fun(view, &Thumbview::select), new Gtk::TreeIter(iter)), 100);
                break;
            }
        }
    }
}

/**
 * Sets the display combobox to the given display value.
 *
 * There will always be a selected item as long as there are items after calling this.
 * 
 * @param   display     The display index to use. If not found, use first available.
 */
void NWindow::set_default_display(int display)
{
    if (!select_display.select_value(this->bg_setter->make_display_key(display)))
        select_display.set_active(0);
}

/////////////////////////////////////////////////////////////////////////////

/**
 * Handles the preferences button being pressed.
 * Shows the preferences dialog and updates the view as needed.
 */
void NWindow::sighandle_btn_prefs()
{
    Config *cfg = Config::get_instance();
    Config *clone = cfg->clone();

    NPrefsWindow *nw = new NPrefsWindow(*this, clone);
    int resp = nw->run();

    if (resp == Gtk::RESPONSE_OK)
    {
        // figure out what directories to reload and what directories to not reload!
        // do this before we reload the main config!
        //
        // if the recurse flag changed, we need to unload/reload everything
        bool recurse_changed = cfg->get_recurse() != clone->get_recurse();

        VecStrs vec_load;
        VecStrs vec_unload;

        VecStrs vec_cfg_dirs = cfg->get_dirs();
        VecStrs vec_clone_dirs = clone->get_dirs();
        for (VecStrs::iterator i = vec_cfg_dirs.begin(); i != vec_cfg_dirs.end(); i++)
            if (recurse_changed || find(vec_clone_dirs.begin(), vec_clone_dirs.end(), *i) == vec_clone_dirs.end())
                vec_unload.push_back(*i);

        for (VecStrs::iterator i = vec_clone_dirs.begin(); i != vec_clone_dirs.end(); i++)
            if (recurse_changed || find(vec_cfg_dirs.begin(), vec_cfg_dirs.end(), *i) == vec_cfg_dirs.end())
                vec_load.push_back(*i);

        cfg->load_cfg();        // tells the global instance to reload itself from disk, which the prefs dialog
                                // told our clone to save to
        view.set_current_display_mode(cfg->get_display_mode());
        view.set_icon_captions(cfg->get_icon_captions());

        for (VecStrs::iterator i = vec_unload.begin(); i != vec_unload.end(); i++)
            view.unload_dir(*i);

        for (VecStrs::iterator i = vec_load.begin(); i != vec_load.end(); i++)
            view.load_dir(*i);

        view.set_sort_mode(cfg->get_sort_mode());
    }

}

