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
#include "Thumbview.h"
#include "md5.h"
#include <glib/gstdio.h>
#include <png.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "Config.h"

// TODO: move this?
struct TreePair {
	Glib::ustring file;
	Gtk::TreeModel::iterator iter;		
};

/**
 * Constructor, sets up gtk stuff, inits data and queues
 */
Thumbview::Thumbview() : dir("") {
	set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	set_shadow_type (Gtk::SHADOW_IN);

	record.add (thumbnail);
	record.add (description);
	record.add (filename);
	record.add (time);

	store = Gtk::ListStore::create (record);
	
	view.set_model (store);
	view.set_headers_visible (FALSE);
	view.set_rules_hint (TRUE);
	
	rend.property_ellipsize () = Pango::ELLIPSIZE_END;
	rend.set_property ("ellipsize", Pango::ELLIPSIZE_END);

	// make the text bold
	rend.property_weight () = Pango::WEIGHT_BOLD;
	
	this->col_thumb = new Gtk::TreeViewColumn("thumbnail", this->rend_img);
	this->col_desc = new Gtk::TreeViewColumn("description", this->rend);
	
	col_desc->add_attribute (rend, "markup", 1);
	col_thumb->pack_start (thumbnail);
	col_desc->set_sort_column (filename);
	col_desc->set_sort_indicator (true);
	col_desc->set_sort_order (Gtk::SORT_ASCENDING);
	
	view.append_column (*col_thumb);
	view.append_column (*col_desc);

	// enable search
	view.set_search_column (description);
	view.set_search_equal_func (sigc::mem_fun (this, &Thumbview::search_compare));

	// enable alphanumeric sorting
	// store->set_sort_column (short_filename, Gtk::SORT_ASCENDING);
	
	col_desc->set_expand ();

	add (view);

	view.show ();
	show ();

	// init async queues
	this->aqueue_thumbs = g_async_queue_new();
	this->aqueue_donethumbs = g_async_queue_new();

	// init dispatcher
	this->dispatch_thumb.connect(sigc::mem_fun(this, &Thumbview::handle_dispatch_thumb));
}

/**
 * Destructor, cleans up asyncqueue ref's
 */
Thumbview::~Thumbview() {
	g_async_queue_unref(this->aqueue_thumbs);
	g_async_queue_unref(this->aqueue_donethumbs);
}

/**
 * Opens the internal directory and starts reading files into the async queue.
 */
void Thumbview::load_dir() {
	
	// grab a ref to our queue
	g_async_queue_ref(this->aqueue_thumbs);

	std::queue<Glib::ustring> subdirs;
	Glib::Dir *dirhandle;	

	// for modified time
	struct stat * buf = new struct stat;
	
	// push the initial dir back onto subdirs 
	subdirs.push(this->dir);
	
	// loop it
	while ( ! subdirs.empty() ) {

		// pop first
		Glib::ustring curdir = subdirs.front();
		subdirs.pop();
		
		try {
			dirhandle = new Glib::Dir(curdir);
			#ifdef PENDEBUG
			std::cout << "DEBUG: Opening dir " << curdir << "\n";
			#endif
		} catch (Glib::FileError e) {
			std::cerr << "Could not open dir " << this->dir << ": " << e.what() << "\n";
			continue;
		}
	
		for (Glib::Dir::iterator i = dirhandle->begin(); i != dirhandle->end(); i++) {
			Glib::ustring fullstr;	
			try {
				fullstr = curdir + Glib::ustring('/') + Glib::filename_to_utf8(*i);
			} catch (Glib::ConvertError& error) {
				std::cerr << "Invalid UTF-8 encountered. Skipping file " << *i << std::endl;
				continue;
			}

			if ( Glib::file_test(fullstr, Glib::FILE_TEST_IS_DIR) )
			{
				if ( Config::get_instance()->get_recurse() )
					subdirs.push(fullstr);
			}
			else {			
				if ( this->is_image(fullstr) ) {

					Gtk::TreeModel::iterator iter = this->store->append ();
					Gtk::TreeModel::Row row = *iter;
					Glib::RefPtr<Gdk::Pixbuf> thumb;

					extern guint8 ni_loading[]; 
					thumb = Gdk::Pixbuf::create_from_inline(24+2993, ni_loading);

					row[thumbnail] = thumb;
					//row[description] = "<b>" + Glib::Markup::escape_text(Glib::ustring(fullstr, fullstr.rfind ("/")+1)) + "</b>";
					row[filename] = fullstr;
					row[description] = Glib::ustring(fullstr, fullstr.rfind ("/")+1);
				
					stat (fullstr.c_str (), buf);
					row[time] = buf->st_mtime;
	
					#ifdef PENDEBUG
					std::cout << "DEBUG: Adding file " << fullstr << "\n";
					#endif
				
					// push it on the thumb queue
					TreePair *tp = new TreePair();
					tp->file = fullstr;
					tp->iter = iter;
					
					g_async_queue_push(this->aqueue_thumbs, (gpointer)tp);
				}
			}
		}

		delete dirhandle;
	}

	// push end signal, unref our queue
	g_async_queue_push(this->aqueue_thumbs, (gpointer)new Glib::ustring("_END_"));
	g_async_queue_unref(this->aqueue_thumbs);

	delete buf;
}

/**
 * Tests the file to see if it is an image
 * TODO: come up with less sux way of doing it than extension
 *
 * @param	file	The filename to test
 * @return			If its an image or not
 */
bool Thumbview::is_image(std::string file) {
	if (file.find (".png")  != std::string::npos ||
		file.find (".PNG")  != std::string::npos || 
		file.find (".jpg")  != std::string::npos || 
		file.find (".JPG")  != std::string::npos || 
		file.find (".jpeg") != std::string::npos || 
		file.find (".JPEG") != std::string::npos ||
		file.find (".gif")  != std::string::npos ||
		file.find (".GIF")  != std::string::npos ) 
		return true;

	return false;
}

/**
 * Determines the full path to a cache file.  Does all creation of dirs.
 * TODO: should throw exception if we cannot create the dirs!
 *
 * @param	file	The file to convert, calls cache_name on it
 * @return			The full path to 
 */
Glib::ustring Thumbview::cache_file(Glib::ustring file) {
	
	Glib::ustring urifile = Glib::filename_to_uri(file);

	// compute md5 of file's uri
	md5_state_t state;
	md5_byte_t digest[16];
	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)urifile.c_str(), strlen(urifile.c_str()));
	md5_finish(&state, digest);

	char *buf = new char[3];
	char *full = new char[33];
	full[0] = '\0';
	
	for (int di = 0; di < 16; ++di) {
		sprintf(buf, "%02x", digest[di]);
		strcat(full, buf);
	}

	Glib::ustring md5file = Glib::ustring(full) + Glib::ustring(".png");
	delete [] buf;
	delete [] full;

	// start at thumbnails dir
	Glib::ustring halfref = Glib::get_home_dir() + Glib::ustring("/.thumbnails/");

	if ( !Glib::file_test(halfref, Glib::FILE_TEST_EXISTS) )
		if ( ! mkdir(halfref.c_str(), 0700) )
			// TODO: exception
			return "FAIL";

	// add normal
	halfref += Glib::ustring("normal/");

	if ( !Glib::file_test(halfref, Glib::FILE_TEST_EXISTS) )
		if ( ! mkdir(halfref.c_str(), 0700) )
			// TODO: exception
			return "FAIL";

	// add and return
	halfref += md5file;
	return halfref;
}

/**
 * Creates cache images that show up in its async queue.  Designed to be run
 * from a thread.
 */
void Thumbview::make_cache_images() {


	// ref our async queues
	g_async_queue_ref(this->aqueue_thumbs);
	g_async_queue_ref(this->aqueue_donethumbs);

	// TODO: consider hcanging to timed_pop and removing _END_ signal
	while (1) {

		Glib::RefPtr<Gdk::Pixbuf> thumb;

		// remove first item
		TreePair *p = (TreePair*)g_async_queue_pop(this->aqueue_thumbs);
		
		// exit condition
		if ( p->file == Glib::ustring("_END_")) {
			delete p;
			break;
		}
		
		Glib::ustring file = p->file;
		Glib::ustring cachefile = this->cache_file(file);

		// branch to see if we need to load or create cache file
		if ( !Glib::file_test(cachefile, Glib::FILE_TEST_EXISTS) ) {

			#ifdef PENDEBUG
			std::cout << "DEBUG: Caching file " << file << "\n";
			#endif

			// open image
			try {
				thumb = Gdk::Pixbuf::create_from_file(file);
			} catch (...) {
				// forget it, move on
				continue;
			}

			// eliminate zero heights (due to really tiny images :/)
			int height = (int)(100*((float)thumb->get_height()/(float)thumb->get_width()));
			if (!height) height = 1;

			// create thumb
			thumb = thumb->scale_simple(100, height, Gdk::INTERP_TILES);

			// create required fd.o png tags
			std::list<Glib::ustring> opts, vals;
			opts.push_back(Glib::ustring("tEXt::Thumb::URI"));
			vals.push_back(Glib::filename_to_uri(file));
			
			struct stat fst;
			stat(file.c_str(), &fst);

			char *bufout = new char[20];
			sprintf(bufout, "%d", fst.st_mtime);

			opts.push_back(Glib::ustring("tEXt::Thumb::MTime"));
			vals.push_back(Glib::ustring(bufout));

			delete [] bufout;
			
			thumb->save(cachefile, "png", opts, vals);

		}
		
		// put in queue
		g_async_queue_push(this->aqueue_donethumbs, (gpointer)p);

		// emit signal
		this->dispatch_thumb.emit();
	}
	
	g_async_queue_unref(this->aqueue_thumbs);
	g_async_queue_unref(this->aqueue_donethumbs);
	throw Glib::Thread::Exit();
}

/**
 * Recieves notification that a thumb is ready for display
 */
void Thumbview::handle_dispatch_thumb() {
	TreePair *p = (TreePair*)g_async_queue_pop(this->aqueue_donethumbs);
	Glib::ustring file = p->file;
	Gtk::TreeModel::iterator iter = p->iter;
	delete p;
	
	Gtk::TreeModel::Row row = *iter;
	Glib::RefPtr<Gdk::Pixbuf> pb;
	pb = Gdk::Pixbuf::create_from_file(this->cache_file(file), 96, 96, true);
	row[thumbnail] = pb;
	
	// build desc
	row[description] = "<b>" + Glib::Markup::escape_text(std::string(file, file.rfind ("/")+1)) + "</b>";

	// emit a changed signal
	store->row_changed (store->get_path(iter), iter);

}

/**
 * Used by GTK to see whether or not an iterator matches a search string.
 */
bool Thumbview::search_compare (const Glib::RefPtr<Gtk::TreeModel>& model, int column, const Glib::ustring& key, const Gtk::TreeModel::iterator& iter) {
	Glib::ustring target = (*iter)[description];
	if (target.find (key) != Glib::ustring::npos) {
		return false;
	}
	return true;
}

/**
 * Sets the sort mode. This tells the view how it should sort backgrounds.
 */
void Thumbview::set_sort_mode (Thumbview::SortMode mode) {
	switch (mode) {
		case SORT_ALPHA:
			store->set_sort_column (description, Gtk::SORT_ASCENDING);
			break;
		case SORT_RALPHA:
			store->set_sort_column (description, Gtk::SORT_DESCENDING);
			break;
		case SORT_TIME:
			store->set_sort_column (time, Gtk::SORT_ASCENDING);
			break;
		case SORT_RTIME:
			store->set_sort_column (time, Gtk::SORT_DESCENDING);
			break;
	}
}

