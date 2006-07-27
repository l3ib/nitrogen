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

// leethax constructor

NWindow::NWindow (void) : apply (Gtk::Stock::APPLY), dosearch(Gtk::Stock::REFRESH), clearsearch(Gtk::Stock::CLEAR), cb_save("Sa_ve"), is_multihead(false)  {
	
	set_border_width (5);
	set_default_size (350, 500);

	main_vbox.set_spacing (5);
	add (main_vbox);

	// save button
	cb_save.set_use_underline(true);
	cb_save.set_active(true);
	
	// setup imagecombos
	this->setup_select_boxes();

	// bottom hbox
	bot_hbox.set_spacing (5);
	bot_hbox.pack_start (select_mode, FALSE, FALSE, 0);
	bot_hbox.pack_start (select_display, FALSE, FALSE, 0);
	bot_hbox.pack_start(button_bgcolor, FALSE, FALSE, 0);
	
	bot_hbox.pack_end(apply, FALSE, FALSE, 0);
	bot_hbox.pack_end(cb_save, FALSE, FALSE, 0);

	// set up search hbox
	search_hbox.set_spacing(5);
	search_hbox.pack_start(search_string, TRUE, TRUE, 0);
	search_hbox.pack_start(dosearch, FALSE, FALSE, 0);
	search_hbox.pack_start(clearsearch, FALSE, FALSE, 0);

	// search expander
	expand_search.set_use_underline();
	expand_search.set_label("_Search");
	expand_search.add(search_hbox);
	
	// add to main box
	main_vbox.pack_start(expand_search, FALSE, FALSE, 0);
	main_vbox.pack_start (view, TRUE, TRUE, 0);
	main_vbox.pack_start (bot_hbox, FALSE, FALSE, 0);

	// signals
	view.view.signal_row_activated ().connect (sigc::mem_fun(*this, &NWindow::sighandle_dblclick_item));
	apply.signal_clicked ().connect (sigc::mem_fun(*this, &NWindow::sighandle_click_apply));
	select_mode.signal_changed().connect(sigc::mem_fun(*this, &NWindow::sighandle_mode_change));

	// set icon
	extern guint8 ni_icon[];
	Glib::RefPtr<Gdk::Pixbuf> icon = Gdk::Pixbuf::create_from_inline(24+32767, ni_icon);
	this->set_icon(icon);
}

// shows all of our widgets

void NWindow::show (void) {
	view.show ();
	select_mode.show ();
	if ( this->is_multihead ) select_display.show();
	apply.show ();
	cb_save.show();
	bot_hbox.show ();
	main_vbox.show ();

	/*
	expand_search.show();
	search_hbox.show();
	dosearch.show();
	clearsearch.show();
	search_string.show();
	button_bgcolor.show();
	*/

	this->set_title("Nitrogen");
}

/**
 * Handles the user double clicking on a row in the treeview.  
 */
void NWindow::sighandle_dblclick_item (const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn * column) {
	
	// find out which image was double clicked
	Gtk::TreeModel::iterator iter = (view.store)->get_iter(path);
	Gtk::TreeModel::Row row = *iter;
	this->set_bg(row[view.filename]);

}

/**
 * Handles the user pressing the apply button.  Grabs the selected items and
 * calls set_bg on it.
 */
void NWindow::sighandle_click_apply (void) {
	
	// find out which image is currently selected
	Gtk::TreeModel::iterator iter = view.view.get_selection()->get_selected ();
	Gtk::TreeModel::Row row = *iter;
	this->set_bg(row[view.filename]);
	
}

/**
 * Handles when the mode combo box is changed.  Brings up the color button when needed.
 */
void NWindow::sighandle_mode_change(void)
{
	SetBG::SetMode mode = SetBG::string_to_mode( this->select_mode.get_active_data() );
	// only show the bg color button if in a mode that would have bg space
	if ( mode == SetBG::SET_CENTER || mode == SetBG::SET_BEST ) {
		button_bgcolor.show();				
	} else {
		button_bgcolor.hide();
	}
}

/**
 * Queries the necessary widgets to get the data needed to set a bg.  It also
 * saves the bg if the save button is checked.
 * 
 * @param	file	The file to set the bg to
 */
void NWindow::set_bg(const Glib::ustring file) {

	// get the data from the active items
	SetBG::SetMode mode = SetBG::string_to_mode( this->select_mode.get_active_data() );
	Glib::ustring thedisp = this->select_display.get_active_data(); 
	Gdk::Color bgcolor = this->button_bgcolor.get_color();

	// set it
	SetBG::set_bg(thedisp, file, mode, bgcolor);
	
	// should we save?
	if (this->cb_save.get_active())
		Config::get_instance()->set_bg(thedisp, file, mode, bgcolor);
}

// leethax destructor
NWindow::~NWindow () {}

/**
 * Creates our ImageCombo boxes
 */
void NWindow::setup_select_boxes() {
		
	extern guint8 wallpaper_center[];
	extern guint8 wallpaper_scale[];
	extern guint8 wallpaper_tile[];
	extern guint8 img_display[];

	// modes
	this->select_mode.add_image_row( Gdk::Pixbuf::create_from_inline(24 + 337, wallpaper_scale), "Scaled", SetBG::mode_to_string(SetBG::SET_SCALE), true );
	this->select_mode.add_image_row( Gdk::Pixbuf::create_from_inline(24 + 333, wallpaper_center), "Centered", SetBG::mode_to_string(SetBG::SET_CENTER), false );
	this->select_mode.add_image_row( Gdk::Pixbuf::create_from_inline(24 + 383, wallpaper_tile), "Tiled", SetBG::mode_to_string(SetBG::SET_TILE), false );
	this->select_mode.add_image_row( Gdk::Pixbuf::create_from_inline(24 + 337, wallpaper_scale), "Best Fit", SetBG::mode_to_string(SetBG::SET_BEST), false );

	// displays
	Glib::RefPtr<Gdk::DisplayManager> manager = Gdk::DisplayManager::get();
	Glib::RefPtr<Gdk::Display> disp	= manager->get_default_display();

	for (int i=0; i<disp->get_n_screens(); i++) {
		Glib::RefPtr<Gdk::Screen> screen = disp->get_screen(i);
		std::ostringstream ostr;
		ostr << "Screen " << i;
		bool on = (i > 0) ? false : true;
		
		this->select_display.add_image_row( Gdk::Pixbuf::create_from_inline (24 + 684, img_display), ostr.str(), screen->make_display_name(), on );
	}

	if ( disp->get_n_screens() > 1 ) 
		this->is_multihead = true;	

	
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
	if( cfg->get_bg_groups(cfg_displays) ) {
	
		SetBG::SetMode m;		
		Gdk::Color c;
		Glib::ustring default_selection;

		// get default display
		Glib::RefPtr<Gdk::DisplayManager> manager = Gdk::DisplayManager::get();
		Glib::RefPtr<Gdk::Display> disp	= manager->get_default_display();
		Glib::RefPtr<Gdk::Screen> screen = disp->get_default_screen();

		// TODO: will be a bug if default has no config entry :P		
		if (!cfg->get_bg(screen->make_display_name(), default_selection, m, c)) {
			// failed. return?
			return;
		}

		// set em!
		button_bgcolor.set_color(c);
		select_mode.select_value( SetBG::mode_to_string(m) );
		
		// iterate through filename list
		for (Gtk::TreeIter iter = view.store->children().begin(); iter != view.store->children().end(); iter++)
		{
			if ( (*iter)[view.filename] == default_selection) {
				view.view.get_selection()->select(iter);
				view.view.scroll_to_row(Gtk::TreeModel::Path(iter), 0.5);
				break;
			}
		}
	}

}
