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

NWindow::NWindow (void) : apply (Gtk::Stock::APPLY), is_multihead(false), is_xinerama(false), btn_prefs(Gtk::Stock::PREFERENCES) {
	
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
	} catch  (Gtk::IconThemeError e) {
		// don't even worry about it!
	}
	
    m_dirty = false;
}

// shows all of our widgets

void NWindow::show (void) {
	view.show ();
	select_mode.show ();
	if ( this->is_multihead ) select_display.show();
	apply.show ();
	bot_hbox.show ();
	main_vbox.show ();
	button_bgcolor.show();
    btn_prefs.show();

	this->set_title("Nitrogen");
}

/**
 * Handles the user double clicking on a row in the view. 
 */
void NWindow::sighandle_dblclick_item (const Gtk::TreeModel::Path& path) {

	// find out which image was double clicked
	Gtk::TreeModel::iterator iter = (view.store)->get_iter(path);
	Gtk::TreeModel::Row row = *iter;
	this->set_bg(row[view.record.Filename]);

    // preview - set dirty flag
    m_dirty = true;
}

/**
 * Handles the user pressing the apply button.  Grabs the selected items and
 * calls set_bg on it. It also saves the bg and closes the application if 
 * the app is not multiheaded, or the full xin desktop is selected.
 */
void NWindow::sighandle_click_apply (void) {
	
	// find out which image is currently selected
	Gtk::TreeModel::iterator iter = view.get_selected ();
	Gtk::TreeModel::Row row = *iter;
    Glib::ustring file = row[view.record.Filename];
	this->set_bg(file);

    // apply - remove dirty flag
    m_dirty = false;

    SetBG::SetMode mode = SetBG::string_to_mode( this->select_mode.get_active_data() );
	Glib::ustring thedisp = this->select_display.get_active_data(); 
	Gdk::Color bgcolor = this->button_bgcolor.get_color();

	// save	
    Config::get_instance()->set_bg(thedisp, file, mode, bgcolor);

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

bool NWindow::on_delete_event(GdkEventAny *event)
{
    if (m_dirty)
    {
        Gtk::MessageDialog dialog(*this, _("You previewed an image without applying it, exit anyway?"), false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);

        int result = dialog.run();
        switch (result)
        {
            case Gtk::RESPONSE_YES:

                break;
            case Gtk::RESPONSE_NO:
                return true;
                break;
        };
    }

    return Gtk::Window::on_delete_event(event);
}

/**
 * Queries the necessary widgets to get the data needed to set a bg.  * 
 * @param	file	The file to set the bg to
 */
void NWindow::set_bg(const Glib::ustring file) {

	// get the data from the active items
	SetBG::SetMode mode = SetBG::string_to_mode( this->select_mode.get_active_data() );
	Glib::ustring thedisp = this->select_display.get_active_data(); 
	Gdk::Color bgcolor = this->button_bgcolor.get_color();

	// set it
#ifdef USE_XINERAMA
	if (this->is_xinerama)
		SetBG::set_bg_xinerama(xinerama_info, xinerama_num_screens, thedisp, file, mode, bgcolor);
	else
#endif
		SetBG::set_bg(thedisp, file, mode, bgcolor);
	
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
	} catch (Gtk::IconThemeError e) {
		genericicon = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, 16, 16);
	}

	try {
		video_display_icon = icontheme->load_icon("video-display", 16, Gtk::ICON_LOOKUP_NO_SVG);
	} catch (Gtk::IconThemeError e) {
		video_display_icon = genericicon;
	}

	// modes
    icon = genericicon;
    this->select_mode.add_image_row( icon, _("Automatic"), SetBG::mode_to_string(SetBG::SET_AUTO), true );
	try {
		icon = icontheme->load_icon("wallpaper-scaled", 16, Gtk::ICON_LOOKUP_NO_SVG);
		if (!icon) icon = genericicon;
	} catch (Gtk::IconThemeError e) {
		icon = genericicon;
	}
	this->select_mode.add_image_row( icon, _("Scaled"), SetBG::mode_to_string(SetBG::SET_SCALE), false );

	try {
		icon = icontheme->load_icon("wallpaper-centered", 16, Gtk::ICON_LOOKUP_NO_SVG);
		if (!icon) icon = genericicon;
	} catch (Gtk::IconThemeError e) {
		icon = genericicon;
	}
	this->select_mode.add_image_row( icon, _("Centered"), SetBG::mode_to_string(SetBG::SET_CENTER), false );

	try {
		icon = icontheme->load_icon("wallpaper-tiled", 16, Gtk::ICON_LOOKUP_NO_SVG);
		if (!icon) icon = genericicon;
	} catch (Gtk::IconThemeError e) {
		icon = genericicon;
	}
	this->select_mode.add_image_row( icon, _("Tiled"), SetBG::mode_to_string(SetBG::SET_TILE), false );

	try {
		icon = icontheme->load_icon("wallpaper-zoomed", 16, Gtk::ICON_LOOKUP_NO_SVG);
		if (!icon) icon = genericicon;
	} catch (Gtk::IconThemeError e) {
		icon = genericicon;
	}

	this->select_mode.add_image_row( icon, _("Zoomed"), SetBG::mode_to_string(SetBG::SET_ZOOM), false );
	this->select_mode.add_image_row( icon, _("Zoomed Fill"), SetBG::mode_to_string(SetBG::SET_ZOOM_FILL), false );

	// displays
	Glib::RefPtr<Gdk::DisplayManager> manager = Gdk::DisplayManager::get();
	Glib::RefPtr<Gdk::Display> disp	= manager->get_default_display();

	if ( disp->get_n_screens() > 1 ) 
	{
		this->is_multihead = true;	

		for (int i=0; i<disp->get_n_screens(); i++) {
			Glib::RefPtr<Gdk::Screen> screen = disp->get_screen(i);
			std::ostringstream ostr;
			ostr << _("Screen") << " " << i;
			bool on = (screen == disp->get_default_screen());
				
			this->select_display.add_image_row( video_display_icon, ostr.str(), screen->make_display_name(), on );

            map_displays[screen->make_display_name()] = ostr.str();
		}

		return;
	}

#ifdef USE_XINERAMA
	// xinerama
	int event_base, error_base;
	int xinerama = XineramaQueryExtension(GDK_DISPLAY_XDISPLAY(disp->gobj()), &event_base, &error_base);
	if (xinerama) {
		xinerama = XineramaIsActive(GDK_DISPLAY_XDISPLAY(disp->gobj()));
		if (xinerama) {
			xinerama_info = XineramaQueryScreens(GDK_DISPLAY_XDISPLAY(disp->gobj()), &xinerama_num_screens);

			if (xinerama_num_screens > 1) {
				this->is_multihead = true;
				this->is_xinerama = true;

				// add the big one
				this->select_display.add_image_row(video_display_icon, _("Full Screen"), "xin_-1", true);

                map_displays["xin_-1"] = _("Full Screen");

				for (int i=0; i<xinerama_num_screens; i++) {
					std::ostringstream ostr, valstr;
					ostr << _("Screen") << " " << xinerama_info[i].screen_number+1;
					valstr << "xin_" << xinerama_info[i].screen_number;
							
					this->select_display.add_image_row(video_display_icon, ostr.str(), valstr.str(), false);

                    map_displays[valstr.str()] = ostr.str();
				}
							
							return;
			}
		}
	}
#endif

	// if we made it here, we do not have any kind of multihead
	// we still need to insert an entry to the display selector or we will die harshly
	
	this->select_display.add_image_row( video_display_icon, _("Default"), disp->get_default_screen()->make_display_name(), true);

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
	if( cfg->get_bg_groups(cfg_displays) ) 
    {	
        SetBG::SetMode m;
        Gdk::Color c;
		Glib::ustring default_selection;
        Glib::ustring file;

		// get default display
        if (this->is_xinerama)
        {
            // we want the lowest numbered xinerama display (slightly hacky)
            int max = 99;
            for (std::vector<Glib::ustring>::iterator i = cfg_displays.begin(); i != cfg_displays.end(); i++)
            {
                if ((*i).substr(0, 4) != "xin_")
                    continue;

                std::stringstream stream((*i).substr(4));
                int newmax;
                stream >> newmax;
                if (newmax < max)
                    max = newmax;
            }

            std::ostringstream ostr;
            ostr << "xin_" << max;
            default_selection = ostr.str();
        }
        else
            default_selection = Gdk::DisplayManager::get()->get_default_display()->get_default_screen()->make_display_name();

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

        VecStrs vec_load;
        VecStrs vec_unload;

        VecStrs vec_cfg_dirs = cfg->get_dirs();
        VecStrs vec_clone_dirs = clone->get_dirs();
        for (VecStrs::iterator i = vec_cfg_dirs.begin(); i != vec_cfg_dirs.end(); i++)
            if (find(vec_clone_dirs.begin(), vec_clone_dirs.end(), *i) == vec_clone_dirs.end())
                vec_unload.push_back(*i);

        for (VecStrs::iterator i = vec_clone_dirs.begin(); i != vec_clone_dirs.end(); i++)
            if (find(vec_cfg_dirs.begin(), vec_cfg_dirs.end(), *i) == vec_cfg_dirs.end())
                vec_load.push_back(*i);

        cfg->load_cfg();        // tells the global instance to reload itself from disk, which the prefs dialog
                                // told our clone to save to
        view.set_current_display_mode(cfg->get_display_mode());

        for (VecStrs::iterator i = vec_unload.begin(); i != vec_unload.end(); i++)
            view.unload_dir(*i);

        for (VecStrs::iterator i = vec_load.begin(); i != vec_load.end(); i++)
            view.load_dir(*i);
    }

}

