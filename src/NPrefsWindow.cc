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

NPrefsWindow::NPrefsWindow(Gtk::Window& parent, Config *cfg) : Gtk::Dialog(_("Preferences"), parent, true, false)
{

	builder = Gtk::Builder::create_from_file("data/prefs.glade");
	builder->get_widget("dirlist", m_list_dirs);

	Gtk::VBox * view_opts_vbox;
	builder->get_widget("view_opts_vbox", view_opts_vbox);
	view_opts_vbox->pack_start(view_type, false, false, 0);
	view_opts_vbox->reorder_child(view_type, 0);

	Gtk::HBox * sort_hbox;
	builder->get_widget("sort_hbox", sort_hbox);
	sort_hbox->pack_start(sort, false, false, 0);

	// view mode combo box
	view_type.append_text(_("Icon View"));
	view_type.append_text(_("List View"));
   
    m_cfg = cfg;
    DisplayMode mode = m_cfg->get_display_mode();

	view_type.set_active_text(mode == ICON ? _("Icon View") : _("List View"));

	// sort order combo box
	sort.append_text(_("filename"));
	sort.append_text(_("date"));

	if (cfg->get_sort_mode() == Thumbview::SORT_ALPHA) sort.set_active_text(_("filename"));
	else if (cfg->get_sort_mode() == Thumbview::SORT_RTIME) sort.set_active_text(_("date"));

    // signal handlers for directory buttons
	Gtk::Button * btn_adddir;
	Gtk::Button * btn_deldir;
	builder->get_widget("add_dir", btn_adddir);
	builder->get_widget("del_dir", btn_deldir);

    btn_adddir->signal_clicked().connect(sigc::mem_fun(this, &NPrefsWindow::sighandle_click_adddir));
    btn_deldir->signal_clicked().connect(sigc::mem_fun(this, &NPrefsWindow::sighandle_click_deldir));

    // fill dir list from config
    Gtk::TreeModelColumnRecord tmcr;
    tmcr.add(m_tmc_dir);

    m_store_dirs = Gtk::ListStore::create(tmcr);
    m_list_dirs->set_model(m_store_dirs);
    m_list_dirs->append_column("Directory", m_tmc_dir);
    m_list_dirs->set_headers_visible(false);

    VecStrs vecdirs = m_cfg->get_dirs();
    for (VecStrs::iterator i = vecdirs.begin(); i != vecdirs.end(); i++)
    {
        Gtk::TreeModel::iterator iter = m_store_dirs->append();
        (*iter)[m_tmc_dir] = *i;
    }

    // layout stuff
	Gtk::Frame * view_opts_frame;
	Gtk::Frame * dir_frame;
	builder->get_widget("view_opts_frame", view_opts_frame);
	builder->get_widget("dir_frame", dir_frame);

	get_vbox()->pack_start(*view_opts_frame, false, false, 5);
	get_vbox()->pack_start(*dir_frame, true, true, 5);

    add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
    set_response_sensitive(Gtk::RESPONSE_CANCEL);
	
    set_default_size(300, 250);

    show_all();
}

/////////////////////////////////////////////////////////////////////////////

void NPrefsWindow::on_response(int response_id)
{
    if (response_id == Gtk::RESPONSE_OK)
    {
        DisplayMode mode = (view_type.get_active_text() == _("Icon View")) ? ICON : LIST;
        m_cfg->set_display_mode(mode);

		if (sort.get_active_text() == "filename") m_cfg->set_sort_mode(Thumbview::SORT_ALPHA);
		else if (sort.get_active_text() == "date") m_cfg->set_sort_mode(Thumbview::SORT_RTIME);

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
    Gtk::TreeIter iter = m_list_dirs->get_selection()->get_selected();
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

