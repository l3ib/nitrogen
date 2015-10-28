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

#ifndef _THUMBVIEW_H_
#define _THUMBVIEW_H_

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

class Thumbview;

class DelayLoadingStore : public Gtk::ListStore
{
	public:
		static Glib::RefPtr<DelayLoadingStore> create(const Gtk::TreeModelColumnRecord& columns)
		{
			return Glib::RefPtr<DelayLoadingStore>(new DelayLoadingStore(columns));
		}

	protected:
		DelayLoadingStore() : Gtk::ListStore() {}
		DelayLoadingStore(const Gtk::TreeModelColumnRecord& columns) : Gtk::ListStore(columns) {}

	protected:
		virtual void 	get_value_vfunc (const iterator& iter, int column, Glib::ValueBase& value) const;

	public:
		void set_queue(GAsyncQueue *queue) { aqueue_loadthumbs = queue; }
		void set_thumbview(Thumbview *view) { thumbview = view; }

	protected:
		GAsyncQueue *aqueue_loadthumbs;
		Thumbview *thumbview;
};

/////////////////////////////////////////////////////////////////////////////

/**
 * Column record for the Thumbview store.
 */
class ThumbviewRecord : public Gtk::TreeModelColumnRecord
{
    public:
        ThumbviewRecord()
        {
            add(Thumbnail);
            add(Description);
            add(Filename);
            add(Time);
            add(LoadingThumb);
            add(CurBGOnDisp);
            add(RootDir);
        }

		Gtk::TreeModelColumn<Glib::ustring> Filename;
		Gtk::TreeModelColumn<Glib::ustring> Description;
		Gtk::TreeModelColumn< Glib::RefPtr<Gdk::Pixbuf> > Thumbnail;
		Gtk::TreeModelColumn<time_t> Time;
		Gtk::TreeModelColumn<bool> LoadingThumb;
        Gtk::TreeModelColumn<Glib::ustring> CurBGOnDisp;
        Gtk::TreeModelColumn<Glib::ustring> RootDir;
};

/////////////////////////////////////////////////////////////////////////////

enum DisplayMode
{
    LIST,
    ICON
};

/////////////////////////////////////////////////////////////////////////////

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

		Glib::RefPtr<DelayLoadingStore> store;
		Gtk::TreeView view;
        Gtk::IconView iview;
        ThumbviewRecord record;

		// dispatcher
		Glib::Dispatcher dispatch_thumb;

		// thread/idle funcs
		void load_cache_images();
		void create_cache_images();
		void load_dir(std::string dir);
        void load_dir(const VecStrs& dirs);
        void unload_dir(std::string dir);

		void set_sort_mode (SortMode mode);
		// search compare function
		bool search_compare (const Glib::RefPtr<Gtk::TreeModel>& model, int column, const Glib::ustring& key, const Gtk::TreeModel::iterator& iter);

        bool select(const Gtk::TreeModel::iterator *iter);

		// loading image
		Glib::RefPtr<Gdk::Pixbuf> loading_image;

        // "cache" of config - maps displays to full filenames
        std::map<Glib::ustring, Glib::ustring> map_setbgs;
        void load_map_setbgs();

        DisplayMode m_curmode;

        void set_current_display_mode(DisplayMode newmode);
        DisplayMode get_current_display_mode() { return m_curmode; }

        void set_icon_captions(gboolean caps);
        gboolean get_icon_captions() { return m_icon_captions; }

        Gtk::TreeModel::iterator get_selected();
        void set_selected(const Gtk::TreePath&, Gtk::TreeModel::const_iterator*);
        sigc::signal<void, const Gtk::TreePath&> signal_selected;

        const Glib::TimeVal& get_last_loaded_time()
            { return m_last_loaded_time; }

	protected:

        Glib::TimeVal m_last_loaded_time;

        void update_last_loaded_time()
            { m_last_loaded_time.assign_current_time(); }

        void sighandle_iview_activated(const Gtk::TreePath& path);
        void sighandle_view_activated(const Gtk::TreePath& path, Gtk::TreeViewColumn *column);

#ifdef USE_INOTIFY
		void file_deleted_callback(std::string filename);
		void file_changed_callback(std::string filename);
		void file_created_callback(std::string filename);
		std::map<std::string, Inotify::Watch*> watches;
#endif

		void add_file(std::string filename, std::string rootdir);
		void handle_dispatch_thumb();

        VecStrs m_list_loaded_rootdirs;

		Gtk::TreeViewColumn *col_thumb;
		Gtk::TreeViewColumn *col_desc;

		Gtk::CellRendererPixbuf rend_img;
		Gtk::CellRendererText rend;

        gboolean m_icon_captions;

		// utility functions
		Glib::ustring cache_file(Glib::ustring file);
		void update_thumbnail(Glib::ustring file, Gtk::TreeModel::iterator iter, Glib::RefPtr<Gdk::Pixbuf> pb);

		// load thumbnail queue
		GAsyncQueue* aqueue_loadthumbs;
		GAsyncQueue* aqueue_createthumbs;
		GAsyncQueue* aqueue_donethumbs;

};

#endif
