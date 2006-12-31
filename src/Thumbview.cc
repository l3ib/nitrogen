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
#include "Util.h"

/**
 * Returns the last modified time of a file.
 * @param file The name of the file to get the modified time for.
 */
static time_t get_file_mtime(std::string file) {
	struct stat buf;
	if (stat(file.c_str(), &buf) == -1) return 0; // error
	return buf.st_mtime;
}

/**
 * Returns the value of the "tEXt::Thumb::MTime" key for fd.o style thumbs.
 * @param pixbuf The pixbuf of the fd.o thumbnail.
 */
static time_t get_fdo_thumbnail_mtime(Glib::RefPtr<Gdk::Pixbuf> pixbuf) {
	std::string mtime_str = pixbuf->get_option("tEXt::Thumb::MTime");
	std::stringstream stream(mtime_str);
	time_t mtime = 0;
	stream >> mtime;
	return mtime;
}

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
	
	col_desc->add_attribute (rend, "text", 1);
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
	
	// make our async queues
	this->aqueue_createthumbs = g_async_queue_new();
	this->aqueue_donethumbs = g_async_queue_new();	

	// init dispatcher 
	this->dispatch_thumb.connect(sigc::mem_fun(this, &Thumbview::handle_dispatch_thumb)); 

	col_desc->set_expand ();

	add (view);

	view.show ();
	show ();
}

/**
 * Destructor
 */
Thumbview::~Thumbview() {
	g_async_queue_unref(this->aqueue_createthumbs);
	g_async_queue_unref(this->aqueue_donethumbs);
}

/**
 * Adds the given file to the tree view and pushes it onto the thumbnail
 * creation queue.
 *
 * @param filename The name of the file to add.
 *
 */
void Thumbview::add_file(std::string filename) {
	Gtk::TreeModel::iterator iter = this->store->append ();
	Gtk::TreeModel::Row row = *iter;
	Glib::RefPtr<Gdk::Pixbuf> thumb = Gtk::IconTheme::get_default()->load_icon("image-loading", 64, Gtk::ICON_LOOKUP_FORCE_SVG);

	row[thumbnail] = thumb;
	row[this->filename] = filename;
	row[description] = Glib::ustring(filename, filename.rfind ("/")+1);

	// for modified time
	row[time] = get_file_mtime(filename);

	Util::program_log("add_file(): Adding file %s\n", filename.c_str());

	// push it on the thumb queue
	TreePair *tp = new TreePair();
	tp->file = filename;
	tp->iter = iter;

	queue_thumbs.push(tp);
}


/**
 * Opens the internal directory and starts reading files into the async queue.
 */
void Thumbview::load_dir(std::string dir) {
	if (!dir.length()) dir = this->dir;

	std::queue<Glib::ustring> subdirs;
	Glib::Dir *dirhandle;	

	// push the initial dir back onto subdirs 
	subdirs.push(dir);
	
	// loop it
	while ( ! subdirs.empty() ) {

		// pop first
		Glib::ustring curdir = subdirs.front();
		subdirs.pop();
		
		try {
			dirhandle = new Glib::Dir(curdir);
			Util::program_log("load_dir(): Opening dir %s\n", curdir.c_str());

		} catch (Glib::FileError e) {
			std::cerr << "Could not open dir " << this->dir << ": " << e.what() << "\n";
			continue;
		}

#ifdef USE_INOTIFY

		// check if we're already monitoring this dir.
		if (watches.find(curdir) == watches.end()) {
			// this dir was successfully opened. monitor it for changes with inotify.
			// the Watch will be cleaned up automatically if the dir is deleted.
			Inotify::Watch * watch = Inotify::Watch::create(curdir);
			if (watch) {
				// no error occurred.

				// emitted when a file is deleted in this dir.
				watch->signal_deleted.connect(sigc::mem_fun(this,
					&Thumbview::file_deleted_callback));
				// emitted when a file is modified or created in this dir.
				watch->signal_write_closed.connect(sigc::mem_fun(this,
					&Thumbview::file_changed_callback));
				// two signals that are emitted when a file is renamed in this dir.
				// the best way to handle this IMO is to remove the file upon receiving
				// 'moved_from', and then to add the file upon receiving 'moved_to'.
				watch->signal_moved_from.connect(sigc::mem_fun(this,
					&Thumbview::file_deleted_callback));
				watch->signal_moved_to.connect(sigc::mem_fun(this,
					&Thumbview::file_changed_callback));
				watch->signal_created.connect(sigc::mem_fun(this,
					&Thumbview::file_created_callback));

				watches[curdir] = watch;
			}
		}

#endif

		for (Glib::Dir::iterator i = dirhandle->begin(); i != dirhandle->end(); i++) {
			Glib::ustring fullstr = curdir + Glib::ustring("/");
			try {
				fullstr += /*Glib::filename_to_utf8(*/*i;//);
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
					add_file(fullstr);
				}
			}
		}

		delete dirhandle;
	}
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

	// build dir paths
	Glib::ustring halfref = Glib::build_filename(Glib::get_home_dir(),".thumbnails/");
	halfref = Glib::build_filename(halfref, "normal/");

	if ( !Glib::file_test(halfref, Glib::FILE_TEST_EXISTS) )
		if ( g_mkdir_with_parents(halfref.c_str(), 0700) == -1)
			// TODO: exception
			return "FAIL";

	// add and return
	return Glib::build_filename(halfref, md5file);
}

/**
 * Creates cache images that show up in its queue.  
 */
bool Thumbview::load_cache_images() {

	// check for exit condition!
	if (this->queue_thumbs.empty()) {
		// create our new idle func
		if (g_async_queue_length(this->aqueue_createthumbs) > 0)
			Glib::Thread::create(sigc::mem_fun(this, &Thumbview::create_cache_images), true);
		return false;
	}

	// remove first item
	TreePair *p = this->queue_thumbs.front();
	this->queue_thumbs.pop();
	
	Glib::ustring file = p->file;
	Glib::ustring cachefile = this->cache_file(file);

	// branch to see if we need to load or create cache file
	if ( !Glib::file_test(cachefile, Glib::FILE_TEST_EXISTS) ) {
		g_async_queue_push(this->aqueue_createthumbs,(gpointer)p);
	} else {
		// load thumb
		Glib::RefPtr<Gdk::Pixbuf> pb = Gdk::Pixbuf::create_from_file(this->cache_file(file), 100, 100, true);
		if (get_fdo_thumbnail_mtime(pb) < get_file_mtime(file)) {
			// the thumbnail is old. we need to make a new one.
			pb.clear();
			g_async_queue_push(this->aqueue_createthumbs,(gpointer)p);
		} else {
			// display it
			this->update_thumbnail(file, p->iter, pb);
			// only delete here
			delete p;	
		}

	}

	return true;
}

/**
 * Thread function to create thumbnail cache images for those that do not exist.
 */
void Thumbview::create_cache_images()
{
 	Glib::RefPtr<Gdk::Pixbuf> thumb;

	g_async_queue_ref(this->aqueue_createthumbs); 
	g_async_queue_ref(this->aqueue_donethumbs); 

	// check for exit condition
	while (g_async_queue_length(this->aqueue_createthumbs) > 0) {

		// remove first item
		TreePair *p = (TreePair*)g_async_queue_pop(this->aqueue_createthumbs);

		// get filenames
		Glib::ustring file = p->file;
		Glib::ustring cachefile = this->cache_file(file);

		Util::program_log("create_cache_images(): Caching file %s\n", file.c_str());

		// open image
		try {
			thumb = Gdk::Pixbuf::create_from_file(file);
		} catch (...) {
			// forget it, move on
			delete p;
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

		time_t mtime = get_file_mtime(file);

		char *bufout = new char[20];
		sprintf(bufout, "%d", mtime);

		opts.push_back(Glib::ustring("tEXt::Thumb::MTime"));
		vals.push_back(Glib::ustring(bufout));

		delete [] bufout;
				
		thumb->save(cachefile, "png", opts, vals);

		// send it to the display
		//this->update_thumbnail(file, p->iter, thumb);
		TreePair *sendp = new TreePair();
		sendp->file = file;
		sendp->iter = p->iter;
		sendp->thumb = thumb;
		g_async_queue_push(this->aqueue_donethumbs, (gpointer)sendp);

		// emit dispatcher
		this->dispatch_thumb.emit();
		

		delete p;
	}

	g_async_queue_unref(this->aqueue_createthumbs); 
	g_async_queue_unref(this->aqueue_donethumbs); 
	throw Glib::Thread::Exit();
}

void Thumbview::handle_dispatch_thumb() { 
	g_async_queue_ref(this->aqueue_donethumbs);

	TreePair *donep = (TreePair*)g_async_queue_pop(this->aqueue_donethumbs);
	this->update_thumbnail(donep->file, donep->iter, donep->thumb);
	delete donep;

	g_async_queue_unref(this->aqueue_donethumbs);
}

/**
 * Updates the treeview to show the passed in thumbnail.
 *
 */
void Thumbview::update_thumbnail(Glib::ustring file, Gtk::TreeModel::iterator iter, Glib::RefPtr<Gdk::Pixbuf> pb)
{
	Gtk::TreeModel::Row row = *iter;
	row[thumbnail] = pb;
	// desc
	row[description] = Glib::ustring(file, file.rfind("/")+1);

	// emit a changed signal
	store->row_changed(store->get_path(iter), iter);	
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

#ifdef USE_INOTIFY

/**
 * Called when a file in a directory being monitored by inotify is deleted.
 * Removes the file from the tree view.
 *
 * @param filename The name of the file to remove.
 */
void Thumbview::file_deleted_callback(std::string filename) {
	if (watches.find(filename) != watches.end()) {
		watches.erase(filename);
		return;
	}
	Gtk::TreeIter iter;
	Gtk::TreeModel::Children children = store->children();
	for (iter = children.begin(); iter != children.end(); iter++) {
		Glib::ustring this_filename = (*iter)[this->filename];
		if (this_filename == filename) {
			// remove this iter.
			store->erase(iter);
			break;
		}
	}
}

/**
 * Called when a file in a directory being monitored by inotify is modified
 * or a new file is created. Adds the file to the tree view.
 *
 * @param filename The name of the file modified or created.
 */
void Thumbview::file_changed_callback(std::string filename) {
	// first remove the old instance of this file.
	file_deleted_callback(filename);
	if ( Glib::file_test(filename, Glib::FILE_TEST_IS_DIR) ) {
		// this is a directory; use load_dir() to set us up the bomb.
		load_dir(filename);
	} else if (is_image(filename)) {
		// this is a file.
		add_file(filename);
	}
	// restart the idle function
	Glib::signal_idle().connect(sigc::mem_fun(this, &Thumbview::load_cache_images));
}

/**
 * Called when a new file or directory is created in a directory being
 * monitored. Discards the event for non-directories, because
 * file_changed_callback will be called for those.
 *
 * @param filename The name of the file modified or created.
 */
void Thumbview::file_created_callback(std::string filename) {
	if ( Glib::file_test(filename, Glib::FILE_TEST_IS_DIR) ) {
		file_changed_callback(filename);
	}
}

#endif
