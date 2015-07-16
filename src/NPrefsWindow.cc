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

NPrefsWindow::NPrefsWindow(Gtk::Window& parent, Config *cfg) : Gtk::Dialog(_("Preferences"), parent, true, false),
                                m_frame_view(_("View Options")),
                                m_frame_dirs(_("Directories")),
                                m_frame_sort(_("Sort by")),
                                m_rb_view_icon(_("_Icon"), true),
                                m_rb_view_icon_caps(_("_Icon with captions"), true),
                                m_rb_view_list(_("_List"), true),
                                m_rb_sort_rtime(_("_Time (descending)"), true),
                                m_rb_sort_time(_("_Time (ascending)"), true),
                                m_rb_sort_alpha(_("_Name (ascending)"), true),
                                m_rb_sort_ralpha(_("_Name (descending)"), true),
                                m_cb_recurse(_("Recurse"), true),
                                m_btn_adddir(Gtk::Stock::ADD),
                                m_btn_deldir(Gtk::Stock::DELETE)
{
    // radio button grouping
    Gtk::RadioButton::Group group = m_rb_view_icon.get_group();
    m_rb_view_icon_caps.set_group(group);
    m_rb_view_list.set_group(group);

    Gtk::RadioButton::Group sort_group = m_rb_sort_alpha.get_group();
    m_rb_sort_rtime.set_group(sort_group);
    m_rb_sort_time.set_group(sort_group);
    m_rb_sort_ralpha.set_group(sort_group);

    m_cfg = cfg;
    DisplayMode mode = m_cfg->get_display_mode();

    if (mode == ICON) {
        if (m_cfg->get_icon_captions())
            m_rb_view_icon_caps.set_active(true);
        else
            m_rb_view_icon.set_active(true);
    }
    else
        m_rb_view_list.set_active(true);

    bool recurse = m_cfg->get_recurse();
    m_cb_recurse.set_active(recurse);

    Thumbview::SortMode sort_mode = m_cfg->get_sort_mode();
    if (sort_mode == Thumbview::SORT_ALPHA)
        m_rb_sort_alpha.set_active(true);
    else if (sort_mode == Thumbview::SORT_RALPHA)
        m_rb_sort_ralpha.set_active(true);
    else if (sort_mode == Thumbview::SORT_TIME)
        m_rb_sort_time.set_active(true);
    else
        m_rb_sort_rtime.set_active(true);

    // signal handlers for directory buttons
    m_btn_adddir.signal_clicked().connect(sigc::mem_fun(this, &NPrefsWindow::sighandle_click_adddir));
    m_btn_deldir.signal_clicked().connect(sigc::mem_fun(this, &NPrefsWindow::sighandle_click_deldir));

    // fill dir list from config
    Gtk::TreeModelColumnRecord tmcr;
    tmcr.add(m_tmc_dir);

    m_store_dirs = Gtk::ListStore::create(tmcr);
    m_list_dirs.set_model(m_store_dirs);
    m_list_dirs.append_column("Directory", m_tmc_dir);
    m_list_dirs.set_headers_visible(false);

    VecStrs vecdirs = m_cfg->get_dirs();
    for (VecStrs::iterator i = vecdirs.begin(); i != vecdirs.end(); i++)
    {
        Gtk::TreeModel::iterator iter = m_store_dirs->append();
        (*iter)[m_tmc_dir] = *i;
    }

    // layout stuff
    m_align_view.set_padding(0, 0, 12, 0);
    m_align_view.add(m_vbox_view);

    m_frame_view.add(m_align_view);
    m_frame_view.set_shadow_type(Gtk::SHADOW_NONE);
    m_vbox_view.pack_start(m_rb_view_icon, false, true);
    m_vbox_view.pack_start(m_rb_view_icon_caps, false, true);
    m_vbox_view.pack_start(m_rb_view_list, false, true);

    m_align_sort.set_padding(0, 0, 12, 0);
    m_align_sort.add(m_vbox_sort);
    m_frame_sort.add(m_align_sort);
    m_frame_sort.set_shadow_type(Gtk::SHADOW_NONE);
    m_vbox_sort.pack_start(m_rb_sort_alpha, false, true);
    m_vbox_sort.pack_start(m_rb_sort_ralpha, false, true);
    m_vbox_sort.pack_start(m_rb_sort_time, false, true);
    m_vbox_sort.pack_start(m_rb_sort_rtime, false, true);

    m_align_dirs.set_padding(0, 0, 12, 0);
    m_align_dirs.add(m_vbox_dirs);

    m_frame_dirs.add(m_align_dirs);
    m_frame_dirs.set_shadow_type(Gtk::SHADOW_NONE);
    m_vbox_dirs.pack_start(m_scrolledwin, true, true);
    m_vbox_dirs.pack_start(m_hbox_dirbtns, false, true);

    m_scrolledwin.add(m_list_dirs);
    m_scrolledwin.set_shadow_type(Gtk::SHADOW_IN);
    m_scrolledwin.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

    m_hbox_dirbtns.pack_start(m_btn_adddir, false, true);
    m_hbox_dirbtns.pack_start(m_btn_deldir, false, true);
    m_hbox_dirbtns.pack_end(m_cb_recurse, false, true);

    get_vbox()->pack_start(m_frame_view, false, true);
    get_vbox()->pack_start(m_frame_sort, false, true);
    get_vbox()->pack_start(m_frame_dirs, true, true);

    add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
    set_response_sensitive(Gtk::RESPONSE_CANCEL);

    set_default_size(350, 350);

    show_all();
}

/////////////////////////////////////////////////////////////////////////////

void NPrefsWindow::on_response(int response_id)
{
    if (response_id == Gtk::RESPONSE_OK)
    {
        DisplayMode mode = (m_rb_view_icon.get_active() || m_rb_view_icon_caps.get_active()) ? ICON : LIST;
        m_cfg->set_display_mode(mode);
        m_cfg->set_icon_captions(m_rb_view_icon_caps.get_active());
        m_cfg->set_recurse((m_cb_recurse.get_active()) ? true : false);
        if (m_rb_sort_alpha.get_active())
            m_cfg->set_sort_mode(Thumbview::SORT_ALPHA);
        else if (m_rb_sort_ralpha.get_active())
            m_cfg->set_sort_mode(Thumbview::SORT_RALPHA);
        else if (m_rb_sort_time.get_active())
            m_cfg->set_sort_mode(Thumbview::SORT_TIME);
        else
            m_cfg->set_sort_mode(Thumbview::SORT_RTIME);

        m_cfg->save_cfg();
    }

    hide();
}

/////////////////////////////////////////////////////////////////////////////

void NPrefsWindow::sighandle_click_adddir()
{
    Gtk::FileChooserDialog dialog("Folder", Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
    dialog.set_transient_for(*this);

    dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    dialog.add_button("Select", Gtk::RESPONSE_OK);

    int result = dialog.run();
    if (result == Gtk::RESPONSE_OK)
    {
        std::string newdir = dialog.get_filename();
        if (m_cfg->add_dir(newdir))
        {
            Gtk::TreeModel::iterator iter = m_store_dirs->append();
            (*iter)[m_tmc_dir] = newdir;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////

void NPrefsWindow::sighandle_click_deldir()
{
    Gtk::TreeIter iter = m_list_dirs.get_selection()->get_selected();
    if (!iter)
        return;

    std::string dir = (*iter)[m_tmc_dir];

    Glib::ustring msg = Glib::ustring::compose(_("Are you sure you want to delete <b>%1</b>?"), dir);

    Gtk::MessageDialog dialog(*this, msg, true, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO);

    int result = dialog.run();
    if (result == Gtk::RESPONSE_YES)
    {
        if (m_cfg->rm_dir(dir))
        {
            m_store_dirs->erase(iter);
        }
    }
}

