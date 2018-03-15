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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "Config.h"
#include "Util.h"
#include "gcs-i18n.h"
#include "string.h"

using namespace Util;

typedef std::pair<Glib::ustring, Glib::ustring> PairStrs;

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

void DelayLoadingStore::get_value_vfunc (const iterator& iter, int column, Glib::ValueBase& value) const
{
	g_async_queue_ref(aqueue_loadthumbs);
	Gtk::ListStore::get_value_vfunc(iter, column, value);

	Gtk::TreeModel::Row row = *iter;
	if (column == 0)
	{
		Glib::Value< Glib::RefPtr<Gdk::Pixbuf> > base;
		Gtk::ListStore::get_value_vfunc(iter, column, base);

		Glib::RefPtr<Gdk::Pixbuf> thumb = base.get();

		if (thumb == thumbview->loading_image && !row[thumbview->record.LoadingThumb])
		{
			TreePair* tp = new TreePair();
			tp->file = row[thumbview->record.Filename];
			tp->iter = iter;

			row[thumbview->record.LoadingThumb] = true;

			//Util::program_log("Custom model: planning on loading %s\n", tp->file.c_str());

			g_async_queue_push(aqueue_loadthumbs, (gpointer)tp);
		}
	}
	g_async_queue_unref(aqueue_loadthumbs);
}

/**
 * Constructor, sets up gtk stuff, inits data and queues
 */
Thumbview::Thumbview()
{
	set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	set_shadow_type (Gtk::SHADOW_IN);

    // load map of setbgs (need this for add_file)
    load_map_setbgs();

	// make our async queues
	this->aqueue_loadthumbs = g_async_queue_new();
	this->aqueue_createthumbs = g_async_queue_new();
	this->aqueue_donethumbs = g_async_queue_new();

	// create store
	store = DelayLoadingStore::create (record);
	store->set_queue(aqueue_loadthumbs);
	store->set_thumbview(this);

	// setup view
    m_curmode = LIST;
    iview.set_model(store);
    iview.signal_item_activated().connect(sigc::mem_fun(*this, &Thumbview::sighandle_iview_activated));

	view.set_model (store);
	view.set_headers_visible (FALSE);
	view.set_fixed_height_mode (TRUE);
	view.set_rules_hint (TRUE);
    view.signal_row_activated().connect(sigc::mem_fun(*this, &Thumbview::sighandle_view_activated));

	// set cell renderer proprties
	rend.property_ellipsize () = Pango::ELLIPSIZE_END;
	rend.set_property ("ellipsize", Pango::ELLIPSIZE_END);
	rend.property_weight () = Pango::WEIGHT_BOLD;

	rend_img.set_fixed_size(105, 82);

	// make treeviewcolumns
	this->col_thumb = new Gtk::TreeViewColumn("thumbnail", this->rend_img);
	this->col_desc = new Gtk::TreeViewColumn("description", this->rend);

	col_thumb->add_attribute (rend_img, "pixbuf", record.Thumbnail);
	col_thumb->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
	col_thumb->set_fixed_width(105);
	col_desc->add_attribute (rend, "markup", record.Description);
	col_desc->set_sort_column (record.Filename);
	col_desc->set_sort_indicator (true);
	col_desc->set_sort_order (Gtk::SORT_ASCENDING);
	col_desc->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);

	view.append_column (*col_thumb);
	view.append_column (*col_desc);

    iview.set_pixbuf_column(record.Thumbnail);
    if (m_icon_captions)
        iview.set_markup_column(record.Description);
    iview.set_tooltip_column(1);
    iview.set_margin(0);
    iview.set_column_spacing(1);
    iview.set_row_spacing(1);

	// enable search
	view.set_search_column (record.Description);
	view.set_search_equal_func (sigc::mem_fun (this, &Thumbview::search_compare));

	// load loading image, which not all themes seem to provide
	try {
		this->loading_image = Gtk::IconTheme::get_default()->load_icon("image-loading", 64, Gtk::ICON_LOOKUP_FORCE_SVG);
	} catch (const Gtk::IconThemeError& e) { std::cerr << "ERR: Could not load \"image-loading\" from icon theme, this indicates a problem with your Gtk/Gtkmm install.\n"; }
      catch (const Gdk::PixbufError& e) { std::cerr << "ERR: Could not load \"image-loading\" from icon theme, this indicates a problem with your Gtk/Gtkmm install.\n"; }

	// init dispatcher
	this->dispatch_thumb.connect(sigc::mem_fun(this, &Thumbview::handle_dispatch_thumb));

	col_desc->set_expand ();

	show_all();
}

/**
 * Destructor
 */
Thumbview::~Thumbview() {
	g_async_queue_unref(this->aqueue_loadthumbs);
	g_async_queue_unref(this->aqueue_createthumbs);
	g_async_queue_unref(this->aqueue_donethumbs);
}

/**
 * Adds the given file to the tree view and pushes it onto the thumbnail
 * creation queue.
 *
 * @param filename The name of the file to add.
 * @param rootdir  The root directory the file came from.
 *
 */
void Thumbview::add_file(std::string filename, std::string rootdir)
{
    Gtk::Window *window = dynamic_cast<Gtk::Window*>(get_toplevel());
	Gtk::TreeModel::iterator iter = this->store->append ();
	Gtk::TreeModel::Row row = *iter;
	Glib::RefPtr<Gdk::Pixbuf> thumb = this->loading_image;
	row[record.Thumbnail] = thumb;
	row[record.Filename] = filename;

    int idx = filename.rfind("/");
	row[record.Description] = Glib::ustring(filename.substr(idx+1));

    row[record.RootDir] = rootdir;

    for (std::map<Glib::ustring, Glib::ustring>::iterator i = map_setbgs.begin(); i!=map_setbgs.end(); i++)
    {
        if (filename == (*i).second)
        {
            row[record.CurBGOnDisp] = (*i).first;
            row[record.Description] = Util::make_current_set_string(window, filename, (*i).first);
        }
    }

	// for modified time
	row[record.Time] = get_file_mtime(filename);

	//Util::program_log("add_file(): Adding file %s (from %s)\n", filename.c_str(), rootdir.c_str());

	// push it on the thumb queue
//	TreePair *tp = new TreePair();
//	tp->file = filename;
//	tp->iter = iter;

//	queue_thumbs.push(tp);
}

void Thumbview::load_dir(std::string dir)
{
    VecStrs dirs;
    dirs.push_back(dir);
    load_dir(dirs);
}

/**
 * Opens the internal directory and starts reading files into the async queue.
 */
void Thumbview::load_dir(const VecStrs& dirs)
{
    for (VecStrs::const_iterator i = dirs.begin(); i != dirs.end(); i++)
    {
        m_list_loaded_rootdirs.push_back(*i);

        std::pair<VecStrs, VecStrs> lists = Util::get_image_files(*i, Config::get_instance()->get_recurse());

        VecStrs file_list = lists.first;
        VecStrs dir_list = lists.second;

        for (VecStrs::const_iterator fi = file_list.begin(); fi != file_list.end(); fi++)
            add_file(*fi, *i);  // file, root dir

#ifdef USE_INOTIFY
        for (VecStrs::const_iterator di = dir_list.begin(); di != dir_list.end(); di++) {
            std::string curdir = *di;

            // check if we're already monitoring this dir.
            if (watches.find(curdir) == watches.end()) {
                // this dir was successfully opened. monitor it for changes with inotify.
                // the Watch will be cleaned up automatically if the dir is deleted.
                Inotify::Watch * watch = Inotify::Watch::create(curdir);
                if (watch) {
                    // no error occurred.

                    // emitted when a file is deleted in this dir.
                    watch->signal_deleted.connect(sigc::mem_fun(this, &Thumbview::file_deleted_callback));
                    // emitted when a file is modified or created in this dir.
                    watch->signal_write_closed.connect(sigc::mem_fun(this, &Thumbview::file_changed_callback));
                    // two signals that are emitted when a file is renamed in this dir.
                    // the best way to handle this IMO is to remove the file upon receiving
                    // 'moved_from', and then to add the file upon receiving 'moved_to'.
                    watch->signal_moved_from.connect(sigc::mem_fun(this, &Thumbview::file_deleted_callback));
                    watch->signal_moved_to.connect(sigc::mem_fun(this, &Thumbview::file_changed_callback));
                    watch->signal_created.connect(sigc::mem_fun(this, &Thumbview::file_created_callback));

                    watches[curdir] = watch;
                }
            }
        }
#endif
    }
}

/**
 * Unloads a directory from the view.
 *
 * This includes all subdirectories if the add was recursive. It looks at the RootDir column
 * in the model to determine if it is a candidate to remove.
 */
void Thumbview::unload_dir(std::string dir)
{
    Gtk::TreeIter iter;
    for (iter = store->children().begin(); iter != store->children().end(); )
    {
        if ((*iter)[record.RootDir] == dir)
            iter = store->erase(iter);
        else
            iter++;
    }
}

/**
 * Sets the selection to the passed in tree iterator.
 * This is a timeout function.
 *
 * Respects the currently selected view mode.
 * @param   iter        The item in the tree model to select.
 */
bool Thumbview::select(const Gtk::TreeModel::iterator *iter)
{
    Gtk::TreeModel::Path path(*iter);

    Glib::TimeVal curtime;
    curtime.assign_current_time();

    // make sure we've given it enough time
    curtime.subtract(m_last_loaded_time);
    if (curtime.as_double() < 0.1)
        return true;


    if (m_curmode == ICON)
    {
        iview.select_path(path);
        iview.scroll_to_path(path, true, 0.5, 0.5);
    }
    else
    {
        view.get_selection()->select(*iter);
        view.scroll_to_row(path, 0.5);
    }

    delete iter;
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
	Glib::ustring halfref = Glib::build_filename(Glib::get_user_cache_dir(),"thumbnails/");
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
void Thumbview::load_cache_images()
{
	g_async_queue_ref(this->aqueue_loadthumbs);
	g_async_queue_ref(this->aqueue_donethumbs);

	while(1)
	{
		// remove first item (blocks until an item occurs)
		TreePair *p = (TreePair*)g_async_queue_pop(this->aqueue_loadthumbs);

		Glib::ustring file = p->file;
		Glib::ustring cachefile = this->cache_file(file);

		// branch to see if we need to load or create cache file
		if ( !Glib::file_test(cachefile, Glib::FILE_TEST_EXISTS) ) {
			g_async_queue_push(this->aqueue_createthumbs,(gpointer)p);
		} else {
			// load thumb
			Glib::RefPtr<Gdk::Pixbuf> pb = Gdk::Pixbuf::create_from_file(this->cache_file(file));

			// resize if we need to
			if (pb->get_width() > 100 || pb->get_height() > 80)
			{
				int pbwidth = pb->get_width();
				int pbheight = pb->get_height();
				float ratio = (float)pbwidth / (float)pbheight;

				int newwidth, newheight;

				if (abs(100 - pbwidth) > abs(80 - pbheight))
				{
					// cap to vertical
					newheight = 80;
					newwidth = newheight * ratio;
				}
				else
				{
					// cap to horiz
					newwidth = 100;
					newheight = newwidth / ratio;
				}

				pb = pb->scale_simple(newwidth, newheight, Gdk::INTERP_NEAREST);
			}

			if (get_fdo_thumbnail_mtime(pb) < get_file_mtime(file)) {
				// the thumbnail is old. we need to make a new one.
				pb.clear();
				g_async_queue_push(this->aqueue_createthumbs,(gpointer)p);
			} else {
				// display it
				TreePair *sendp = new TreePair();
				sendp->file = file;
				sendp->iter = p->iter;
				sendp->thumb = pb;

				g_async_queue_push(this->aqueue_donethumbs, (gpointer)sendp);

				this->dispatch_thumb.emit();

				delete p;
			}
		}
	}

	g_async_queue_unref(this->aqueue_loadthumbs);
	g_async_queue_unref(this->aqueue_donethumbs);
	throw Glib::Thread::Exit();
}

/**
 * Thread function to create thumbnail cache images for those that do not exist.
 */
void Thumbview::create_cache_images()
{
	Glib::RefPtr<Gdk::Pixbuf> thumb;

	g_async_queue_ref(this->aqueue_createthumbs);
	g_async_queue_ref(this->aqueue_donethumbs);

	// always work!
	while (1) {

		// remove first item (blocks when no items occur)
		TreePair *p = (TreePair*)g_async_queue_pop(this->aqueue_createthumbs);

		// get filenames
		Glib::ustring file = p->file;
		Glib::ustring cachefile = this->cache_file(file);

		Util::program_log("create_cache_images(): + Caching file %s\n", file.c_str());

		// open image, scale to smallish 4:3 with aspect preserved
		try {
			thumb = Gdk::Pixbuf::create_from_file(file, 100, 75, true);
		} catch (...) {
			// forget it, move on
			delete p;
			continue;
		}

		// create required fd.o png tags
		std::list<Glib::ustring> opts, vals;
		opts.push_back(Glib::ustring("tEXt::Thumb::URI"));
		vals.push_back(Glib::filename_to_uri(file));

		time_t mtime = get_file_mtime(file);

		char *bufout = new char[20];
		sprintf(bufout, "%ld", mtime);

		opts.push_back(Glib::ustring("tEXt::Thumb::MTime"));
		vals.push_back(Glib::ustring(bufout));

		delete [] bufout;

		thumb->save(cachefile, "png", opts, vals);

        Util::program_log("create_cache_images(): - Finished caching file %s\n", file.c_str());

		// send it to the display
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

    update_last_loaded_time();

	g_async_queue_unref(this->aqueue_donethumbs);
}

/**
 * Updates the treeview to show the passed in thumbnail.
 *
 */
void Thumbview::update_thumbnail(Glib::ustring file, Gtk::TreeModel::iterator iter, Glib::RefPtr<Gdk::Pixbuf> pb)
{
	Gtk::TreeModel::Row row = *iter;
	row[record.Thumbnail] = pb;
	// desc
	//row[record.Description] = Glib::ustring(file, file.rfind("/")+1);

	// emit a changed signal
	store->row_changed(store->get_path(iter), iter);
}

/**
 * Used by GTK to see whether or not an iterator matches a search string.
 */
bool Thumbview::search_compare (const Glib::RefPtr<Gtk::TreeModel>& model, int column, const Glib::ustring& key, const Gtk::TreeModel::iterator& iter) {
	Glib::ustring target = (*iter)[record.Description];
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
			store->set_sort_column (record.Description, Gtk::SORT_ASCENDING);
			break;
		case SORT_RALPHA:
			store->set_sort_column (record.Description, Gtk::SORT_DESCENDING);
			break;
		case SORT_TIME:
			store->set_sort_column (record.Time, Gtk::SORT_ASCENDING);
			break;
		case SORT_RTIME:
			store->set_sort_column (record.Time, Gtk::SORT_DESCENDING);
			break;
	}
}

/**
 * Loads the map of displays and their set bgs. Used to indicate which
 * files are currently set as a background.
 */
void Thumbview::load_map_setbgs()
{
    map_setbgs.clear();

    std::vector<Glib::ustring> vecgroups;
    bool ret = Config::get_instance()->get_bg_groups(vecgroups);
    if (!ret)
        return;

    for (std::vector<Glib::ustring>::iterator i = vecgroups.begin(); i!=vecgroups.end(); i++)
    {
        Glib::ustring file;
        SetBG::SetMode mode;
        Gdk::Color bgcolor;

        ret = Config::get_instance()->get_bg(*i, file, mode, bgcolor);
        if (!ret)
        {
            std::cerr << "(load_map_setbgs) Could not get background stored info for " << *i << "\n";
            return;
        }

        map_setbgs[*i] = file;
    }
}

/**
 * Sets the current selection with all triggers.
 */
void Thumbview::set_selected(const Gtk::TreePath& path, Gtk::TreeModel::const_iterator *iter)
{
    // visually select
    this->select(new Gtk::TreeModel::const_iterator(*iter));

    // now activate
    if (m_curmode == ICON)
        iview.item_activated(path);
    else
        view.row_activated(path, *this->col_thumb);
}

/**
 * Gets the currently selected image, regardless of which view it came from.
 */
Gtk::TreeModel::iterator Thumbview::get_selected()
{
    if (m_curmode == ICON)
    {
        std::list<Gtk::TreePath> selected = iview.get_selected_items();
        if (selected.size() == 0)
            return store->children().end();

        return store->get_iter(*(selected.begin()));
    }
    else
    {
        return view.get_selection()->get_selected();
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
		Glib::ustring this_filename = (*iter)[record.Filename];
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
        std::string rootdir = "";
		add_file(filename, rootdir);
	}
	// restart the idle function
	//Glib::signal_idle().connect(sigc::mem_fun(this, &Thumbview::load_cache_images));
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

void Thumbview::set_current_display_mode(DisplayMode newmode)
{
    remove();
    if (newmode == LIST)
        add(view);
    else if (newmode == ICON)
        add(iview);

    m_curmode = newmode;
    show_all();
}

void Thumbview::set_icon_captions(gboolean caps)
{
    m_icon_captions = caps;
    if (m_icon_captions)
        iview.set_markup_column(record.Description);
    else
        iview.set_markup_column(-1);  // -1 unsets
}

void Thumbview::sighandle_iview_activated(const Gtk::TreePath& path)
{
    signal_selected(path);
}

void Thumbview::sighandle_view_activated(const Gtk::TreePath& path, Gtk::TreeViewColumn *column)
{
    signal_selected(path);
}


