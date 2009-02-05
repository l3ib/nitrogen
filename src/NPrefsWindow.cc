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

#include "NPrefsWindow.h"
#include "Config.h"
#include "Util.h"
#include "gcs-i18n.h"

NPrefsWindow::NPrefsWindow(Gtk::Window& parent) : Gtk::Dialog(_("Preferences"), parent, true, false), 
                                m_frame_view(_("View Options")),
                                m_frame_dirs(_("Directories")),
                                m_rb_view_icon(_("_Icon"), true),
                                m_rb_view_list(_("_List"), true),
                                m_btn_adddir(Gtk::Stock::ADD),
                                m_btn_deldir(Gtk::Stock::DELETE)
{
    // radio button grouping
    Gtk::RadioButton::Group group = m_rb_view_icon.get_group();
    m_rb_view_list.set_group(group);
   
    Config *cfg = Config::get_instance(); 
    DisplayMode mode = cfg->get_display_mode();

    if (mode == ICON)
        m_rb_view_icon.set_active(true);
    else
        m_rb_view_list.set_active(true);
    
    // layout stuff
    m_align_view.set_padding(0, 0, 12, 0);
    m_align_view.add(m_vbox_view);

    m_frame_view.add(m_align_view);
    m_frame_view.set_shadow_type(Gtk::SHADOW_NONE);
    m_vbox_view.pack_start(m_rb_view_icon, false, true);
    m_vbox_view.pack_start(m_rb_view_list, false, true);

    m_align_dirs.set_padding(0, 0, 12, 0);
    m_align_dirs.add(m_vbox_dirs);

    m_frame_dirs.add(m_align_dirs);
    m_frame_dirs.set_shadow_type(Gtk::SHADOW_NONE);
    m_vbox_dirs.pack_start(m_scrolledwin, true, true);
    m_vbox_dirs.pack_start(m_hbox_dirbtns, false, true);

    m_scrolledwin.add(m_list_dirs);
    m_scrolledwin.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

    m_hbox_dirbtns.pack_start(m_btn_adddir, false, true);
    m_hbox_dirbtns.pack_start(m_btn_deldir, false, true);

    get_vbox()->pack_start(m_frame_view, false, true);
    get_vbox()->pack_start(m_frame_dirs, true, true);

    add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
    set_response_sensitive(Gtk::RESPONSE_CANCEL);
	
    set_default_size (300, 250);

    show_all();
}

/////////////////////////////////////////////////////////////////////////////

void NPrefsWindow::on_response(int response_id)
{
    if (response_id == Gtk::RESPONSE_OK)
    {
        Config *cfg = Config::get_instance(); 
        DisplayMode mode = (m_rb_view_icon.get_active()) ? ICON : LIST;
        cfg->set_display_mode(mode);

        cfg->save_cfg();
    }

    hide();
}


