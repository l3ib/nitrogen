/*

This file is from Nitrogen, an X11 background setter.
Copyright (C) 2009  Dave Foster & Javeed Shaikh

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

#ifndef _NPREFSWINDOW_H_
#define _NPREFSWINDOW_H_

#include "main.h"
#include "Config.h"

class NPrefsWindow : public Gtk::Dialog
{
    public:
        NPrefsWindow(Gtk::Window& parent, Config* cfg);
        virtual ~NPrefsWindow() {}

    protected:
        virtual void on_response(int response_id);
    protected:
        Gtk::VBox m_vbox_view;
        Gtk::VBox m_vbox_dirs;
        Gtk::VBox m_vbox_sort;
        Gtk::HBox m_hbox_dirbtns;
        Gtk::Frame m_frame_view;
        Gtk::Frame m_frame_dirs;
        Gtk::Frame m_frame_sort;
        Gtk::Alignment m_align_view;
        Gtk::Alignment m_align_dirs;
        Gtk::Alignment m_align_sort;
        Gtk::RadioButton m_rb_view_icon;
        Gtk::RadioButton m_rb_view_icon_caps;
        Gtk::RadioButton m_rb_view_list;
        Gtk::RadioButton m_rb_sort_alpha;
        Gtk::RadioButton m_rb_sort_ralpha;
        Gtk::RadioButton m_rb_sort_time;
        Gtk::RadioButton m_rb_sort_rtime;
        Gtk::CheckButton m_cb_recurse;

        Gtk::ScrolledWindow m_scrolledwin;
        Gtk::TreeView m_list_dirs;
        Gtk::Button m_btn_adddir;
        Gtk::Button m_btn_deldir;
        Config* m_cfg;

        // handlers
        void sighandle_click_adddir();
        void sighandle_click_deldir();

        // tree view noise
        Glib::RefPtr<Gtk::ListStore> m_store_dirs;
        Gtk::TreeModelColumn<std::string> m_tmc_dir;
};

#endif
