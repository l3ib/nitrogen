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

class ImageCombo : public Gtk::ComboBox {

	public:
		ImageCombo();
		~ImageCombo();

		void add_image_row(Glib::RefPtr<Gdk::Pixbuf> img, Glib::ustring desc, Glib::ustring data, bool active);
		Glib::ustring get_active_data();

		bool select_value(Glib::ustring);

	protected:

		Gtk::TreeModelColumn<Glib::ustring> tmc_data;
		Gtk::TreeModelColumn< Glib::RefPtr<Gdk::Pixbuf> >  tmc_image;
		Gtk::TreeModelColumn<Glib::ustring> tmc_desc;

		Gtk::TreeModel::ColumnRecord colrecord;

		Gtk::TreeViewColumn tvc_col_img;
		Gtk::TreeViewColumn tvc_col_desc;

		Glib::RefPtr<Gtk::ListStore> store;

		Gtk::CellRendererPixbuf rend_img;
		Gtk::CellRendererText rend_text;
};
