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

#include "ImageCombo.h"

/**
 * Constructor, initializes renderers, columns, etc
 */
ImageCombo::ImageCombo() {

	// add tmcs to the record
	this->colrecord.add( this->tmc_image );
	this->colrecord.add( this->tmc_desc );
	this->colrecord.add( this->tmc_data );

	// init liststore
	this->store = Gtk::ListStore::create( this->colrecord );
	this->set_model( store );

	this->pack_start( this->rend_img, FALSE );
	this->pack_start( this->rend_text );

	this->add_attribute( this->rend_text, "text", 1 );
	this->add_attribute( this->rend_img, "pixbuf", 0 );

}

/**
 * Do nothing!
 */
ImageCombo::~ImageCombo() {

}

/**
 * Adds a row to the combo box.
 *
 * @param	img		The pixbuf to show for this row
 * @param	desc	The text label to display alongside it
 * @param	data	Textual data to store in this row
 * @param	active	Whether to set this row as active or not
 */
void ImageCombo::add_image_row(Glib::RefPtr<Gdk::Pixbuf> img, Glib::ustring desc, Glib::ustring data, bool active) {

	Gtk::TreeModel::iterator iter = this->store->append ();
	Gtk::TreeModel::Row row = *iter;

	row[this->tmc_image] = img;
	row[this->tmc_desc] = desc;
	row[this->tmc_data] = data;

	if ( active )
		this->set_active(iter);
}

/**
 * Returns the data field of the selected row.
 *
 * @return	Exactly what I said above.
 */
Glib::ustring ImageCombo::get_active_data() {
	Gtk::TreeModel::Row arow = *(this->get_active());
	return arow[this->tmc_data];
}

/**
 * Selects the entry with the value specified.
 *
 * @param	value	The value to search for and select.
 * @returns         If the value was able to be set.
 */
bool ImageCombo::select_value(Glib::ustring value)
{
	for (Gtk::TreeIter iter = store->children().begin(); iter != store->children().end(); iter++) {

		if ( (*iter)[tmc_data] == value ) {
			set_active(iter);
            return true;
		}
	}

    return false;
}
