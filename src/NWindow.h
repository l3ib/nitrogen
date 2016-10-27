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

#ifndef _NWINDOW_H_
#define _NWINDOW_H_

#include "main.h"
#include "Thumbview.h"
#include "ImageCombo.h"
#include "SetBG.h"

class NWindow : public Gtk::Window {
    public:
        NWindow(SetBG* bg_setter);
        void show (void);
        virtual ~NWindow ();

        Thumbview view;
        void sighandle_dblclick_item(const Gtk::TreeModel::Path& path);
        void sighandle_click_apply(void);
        void sighandle_mode_change(void);

        void set_default_selections();

        std::map<Glib::ustring, Glib::ustring> map_displays;        // a map of current displays on the running instance to their display names
        void set_default_display(int display);

        bool set_bg(Glib::ustring file);

    protected:
        Glib::RefPtr<Gtk::ActionGroup> m_action_group;
        Glib::RefPtr<Gtk::UIManager> m_ui_manager;
        Gtk::VBox main_vbox;
        Gtk::HBox bot_hbox;
        SetBG* bg_setter;

        ImageCombo select_mode, select_display;

        Gtk::Button apply;
        Gtk::Button btn_prefs;
        Gtk::ColorButton button_bgcolor;

        bool m_dirty;       // set if the user double clicks to preview but forgets to press apply

        void setup_select_boxes();

        void sighandle_accel_quit();
        void sighandle_random();
        void sighandle_togb_list_toggled();
        void sighandle_togb_icon_toggled();
        void sighandle_btn_prefs();
        void apply_bg();

        virtual bool on_delete_event(GdkEventAny *event);
        bool handle_exit_request();

#ifdef USE_XINERAMA
	// xinerama stuff
	XineramaScreenInfo* xinerama_info;
	gint xinerama_num_screens;
#endif
};

#endif
