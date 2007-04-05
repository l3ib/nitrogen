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
#include <queue>
#include <errno.h>

#ifdef USE_INOTIFY
#include "Inotify.h"
#endif

struct TreePair {
	Glib::ustring file;
	Gtk::TreeModel::iterator iter;	
	Glib::RefPtr<Gdk::Pixbuf> thumb;
};

class Thumbview : public Gtk::ScrolledWindow {
	public:
		Thumbview ();
		~Thumbview ();

		typedef enum {
			SORT_ALPHA,
			SORT_RALPHA,
			SORT_TIME,
			SORT_RTIME
		} SortMode;
		
		void set_dir(std::string indir) { this->dir = indir; }
		
		// TODO: make private?
		Gtk::TreeModelColumn<Glib::ustring> filename;
		Gtk::TreeModelColumn<Glib::ustring> description;
		Gtk::TreeModelColumn< Glib::RefPtr<Gdk::Pixbuf> >  thumbnail;
		Gtk::TreeModelColumn<time_t> time;
		
		Glib::RefPtr<Gtk::ListStore> store;
		Gtk::TreeView view;

		// dispatcher 
		Glib::Dispatcher dispatch_thumb;

		// thread/idle funcs
		bool load_cache_images();
		void create_cache_images();
		void load_dir(std::string dir = "");

		void set_sort_mode (SortMode mode);
		// search compare function
		bool search_compare (const Glib::RefPtr<Gtk::TreeModel>& model, int column, const Glib::ustring& key, const Gtk::TreeModel::iterator& iter);
		
	protected:

#ifdef USE_INOTIFY
		void file_deleted_callback(std::string filename);
		void file_changed_callback(std::string filename);
		void file_created_callback(std::string filename);
		std::map<std::string, Inotify::Watch*> watches;
#endif

		void add_file(std::string filename);
		void handle_dispatch_thumb();

		Gtk::TreeModel::ColumnRecord record;
		
		Gtk::TreeViewColumn *col_thumb;
		Gtk::TreeViewColumn *col_desc;
		
		Gtk::CellRendererPixbuf rend_img;
		Gtk::CellRendererText rend;

		// utility functions
		bool is_image(std::string file);
		Glib::ustring cache_file(Glib::ustring file);
		void update_thumbnail(Glib::ustring file, Gtk::TreeModel::iterator iter, Glib::RefPtr<Gdk::Pixbuf> pb);
		
		// base dir
		// TODO: remove when we get a proper db
		std::string dir;

		// load thumbnail queue
		std::queue<TreePair*> queue_thumbs;
		GAsyncQueue* aqueue_createthumbs;
		GAsyncQueue* aqueue_donethumbs;

		// loading image
		Glib::RefPtr<Gdk::Pixbuf> loading_image;
};
