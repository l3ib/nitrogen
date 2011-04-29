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

#include "Config.h"
#include <glib/gstdio.h>
#include "gcs-i18n.h"

/**
 * Constructor, initializes default settings.
 */
Config::Config()
{
	recurse = true;

    m_display_mode = ICON;
	m_sort_mode = Thumbview::SORT_RTIME;

    m_posx = -1;
    m_posy = -1;

    m_sizex = 450;
    m_sizey = 500;

    m_icon_captions = false;
}

/**
 * Singleton retreiver.  NOT THREAD SAFE.  Call this func early to create, then it's ok.
 *
 * @return	A pointer to the one config instance.
 */
Config* Config::get_instance()
{
	// instance
	static Config* _instance;

	if (!_instance)
		_instance = new Config();

	return _instance;
}

/**
 * Creates a clone of this Config instance. Used so the preferences dialog may manipulate the
 * configuration without disturbing the real one.
 */
Config* Config::clone()
{
    Config* retVal = new Config();

    retVal->recurse = recurse;
    retVal->m_display_mode = m_display_mode;
    retVal->m_sort_mode = m_sort_mode;
    retVal->m_posx = m_posx;
    retVal->m_posy = m_posy;
    retVal->m_sizex = m_sizex;
    retVal->m_sizey = m_sizey;
    retVal->m_icon_captions = m_icon_captions;
    retVal->m_vec_dirs = VecStrs(m_vec_dirs);

    return retVal;
}

/**
 * Returns the bg information for a previously stored one
 *
 * @param	disp	The display string to use
 * @param	file	Returns the file thats in there
 * @param	mode	Returns the mode thats in there
 * @param	bgcolor	Returns the bg color thats in there, if any, or a default Gdk::Color if none
 * @return			If it could find the entry
 */
bool Config::get_bg(const Glib::ustring disp, Glib::ustring &file, SetBG::SetMode &mode, Gdk::Color &bgcolor) {

	if ( ! Config::check_dir() ) {
		std::cerr << _("Could not open config directory.") << std::endl;
		return false;
	}

	Glib::ustring cfgfile = Glib::ustring(g_get_user_config_dir()) + Glib::ustring("/nitrogen/bg-saved.cfg");
	GKeyFile *kf = g_key_file_new();
	
	GError *ge = NULL;
	if ( g_key_file_load_from_file(kf, cfgfile.c_str(), G_KEY_FILE_NONE, &ge) == FALSE ) {
		
		std::cerr << _("ERROR: Unable to load config file") << " (" << cfgfile << "): " << ge->message << "\n";
		g_clear_error(&ge);
		g_key_file_free(kf);
		return false;
	}

	// check if the group for this display exists
	if ( ! g_key_file_has_group(kf, disp.c_str()) ) {
		// there is no config entry for this display.
		// abort.
		g_key_file_free(kf);
		std::cerr << _("Couldn't find group for") << " " << disp << "." << std::endl;
		return false;
	}
	
	// get data
	
	gchar *fileptr = g_key_file_get_string(kf, disp.c_str(), "file", NULL);
	if ( fileptr ) {
		file = Glib::ustring(fileptr);
		free(fileptr);
	} else {
		g_key_file_free(kf);
		std::cerr << _("Could not get filename from config file.") << std::endl;
		return false;
	}
		//error = true;
	
	mode = (SetBG::SetMode)g_key_file_get_integer(kf, disp.c_str(), "mode", &ge);
	if (ge) {
		g_clear_error(&ge);
		g_key_file_free(kf);
		std::cerr << _("Could not get mode from config file.") << std::endl;
		return false;
	}

	Glib::ustring tcol("#000000");
	gchar *color = g_key_file_get_string(kf, disp.c_str(), "bgcolor", &ge);
	if (ge) {
		g_clear_error(&ge);
		// This is not a critical error.
		//g_key_file_free(kf);
		//std::cerr << "Could not get bg color from config file." << std::endl;
		//return false;
	} else {
		// worked properly
		tcol = color;
		free(color);
	}
	
	// did not fail
	// Glib::ustring tcol(color);
	// free(color);
	bgcolor.set(tcol);
	
	g_key_file_free(kf);


	// this function looks like less of an eyesore without all the
	// failed trix.
	
	return true; // success
}

/**
 * Sets the specified bg in the config file.
 *
 * @param	disp	The display this one is for
 * @param	file	The bg file to set
 * @param	mode	The mode the bg is set in
 * @param	bgcolor	The background color, ignored depending on mode
 * @return			Success
 */
bool Config::set_bg(const Glib::ustring disp, const Glib::ustring file, const SetBG::SetMode mode, const Gdk::Color bgcolor) {

	if ( ! Config::check_dir() )
		return false;

	Glib::ustring realdisp = disp;
	Glib::ustring cfgfile = Glib::build_filename(Glib::ustring(g_get_user_config_dir()), Glib::ustring("nitrogen"));
	cfgfile = Glib::build_filename(cfgfile, Glib::ustring("bg-saved.cfg"));

	GKeyFile *kf = g_key_file_new();

	GError *ge = NULL;
	if ( g_key_file_load_from_file(kf, cfgfile.c_str(), G_KEY_FILE_NONE, &ge) == FALSE ) {

		// only give an error if it doesn't exist
		if ( ! ( ge->code == G_FILE_ERROR_NOENT || ge->code == G_KEY_FILE_ERROR_NOT_FOUND ) ) {
			std::cerr << _("ERROR: Unable to load config file") << " (" << cfgfile << "): " << ge->message << "\n";
			#ifdef DEBUG
			std::cerr << _("The error code returned was") << " " << ge->code << "\n";
			std::cerr << _("We expected") << " " << G_FILE_ERROR_NOENT << " " << _("or") << " " << G_KEY_FILE_ERROR_NOT_FOUND << "\n";
			#endif
			g_key_file_free(kf);
			g_clear_error(&ge);
			return false;
		}
		g_clear_error(&ge);
	}

	// must do complex removals if xinerama occurs
	if (realdisp.find("xin_", 0, 4) != Glib::ustring::npos) {
		
		if (realdisp == "xin_-1") {
			// get all keys, remove all keys that exist with xin_ prefixes
			gchar **groups;
			gsize num;
			groups = g_key_file_get_groups(kf, &num);
			for (int i=0; i<num; i++)
				if (Glib::ustring(groups[i]).find("xin_", 0, 4) != Glib::ustring::npos)
					g_key_file_remove_group(kf, groups[i], NULL);

		} else {
			
			// a normal xinerama screen, therefore
			// remove the big realdisp if it occurs
			if (g_key_file_has_group(kf, "xin_-1"))
				g_key_file_remove_group(kf, "xin_-1", NULL);
		}
	}


	// set data
	g_key_file_set_string(kf, realdisp.c_str(), "file", file.c_str());
	g_key_file_set_integer(kf, realdisp.c_str(), "mode", (gint)mode);
	g_key_file_set_string(kf, realdisp.c_str(), "bgcolor", color_to_string(bgcolor).c_str());
	
	// output it
	Glib::ustring outp = g_key_file_to_data(kf, NULL, NULL);
	std::ofstream outf(cfgfile.c_str());
	outf << outp;
	outf.close();
	
	g_key_file_free(kf);

	return true;
}

/**
 * Grabs the list of set groups in the bg config file.
 *
 * @param	groups	A vector of strings, each corresponding to a group name
 * @return			If it was able to open and do everything ok
 */
bool Config::get_bg_groups(std::vector<Glib::ustring> &groups) {
		
	if ( ! Config::check_dir() )
		return false;

	Glib::ustring cfgfile = Glib::build_filename(Glib::ustring(g_get_user_config_dir()), Glib::ustring("nitrogen"));
	cfgfile = Glib::build_filename(cfgfile, Glib::ustring("bg-saved.cfg"));
	GKeyFile *kf = g_key_file_new();
	
	if ( g_key_file_load_from_file(kf, cfgfile.c_str(), G_KEY_FILE_NONE, NULL) == FALSE ) {
		
		// do not output anything here, we just want to know
		g_key_file_free(kf);
		return false;
	}

	std::vector<Glib::ustring> res;
	gsize num;
	gchar** filegroups = g_key_file_get_groups(kf, &num);

	// drop into vector
	for (unsigned int i=0; i<num; i++) {
		res.push_back(Glib::ustring(filegroups[i]));
	}
	
	g_strfreev(filegroups);
	groups = res;
	
	g_key_file_free(kf);
	return true;
}

/**
 * Checks to make sure the config dir exists.
 *
 * @return	If it exists, or it was able to make it
 */
bool Config::check_dir() {
		
	Glib::ustring dirname = Glib::build_filename(Glib::ustring(g_get_user_config_dir()), "nitrogen");
	if ( !Glib::file_test(dirname, Glib::FILE_TEST_EXISTS ) )
		if ( g_mkdir_with_parents(dirname.c_str(), 0755) == -1 )
			// TODO: throw
			return false;
	
	return true;
}

/**
 * Converts a Gdk::Color to a string representation with a #.
 *
 * @param	color	The color to convert
 * @return			A hex string
 */
Glib::ustring Config::color_to_string(Gdk::Color color) {
	guchar red = guchar(color.get_red_p() * 255);
	guchar green = guchar(color.get_green_p() * 255);
	guchar blue = guchar(color.get_blue_p() * 255);

	char * c_str = new char[7];
	
	snprintf(c_str, 7, "%.2x%.2x%.2x", red, green, blue);
	Glib::ustring string = '#' + Glib::ustring(c_str);
	
	delete[] c_str;
	return string;
}

/**
 * Gets the last saved position.
 */
void Config::get_pos(gint &x, gint &y)
{
    x = m_posx;
    y = m_posy;
}

/**
 * Sets the position, to be saved with save_cfg.
 */
void Config::set_pos(gint x, gint y)
{
    m_posx = x;
    m_posy = y;
}

/**
 * Gets the last saved window size.
 */
void Config::get_size(guint &w, guint &h)
{
    w = m_sizex;
    h = m_sizey;
}

/**
 * Sets the window size, to be saved with save_cfg.
 */
void Config::set_size(guint w, guint h)
{
    m_sizex = w;
    m_sizey = h;
}

/**
 * Saves common configuration params to a configuration file.
 *
 * Stored in ~/.config/nitrogen/nitrogen.cfg
 */
bool Config::save_cfg()
{
	if ( ! Config::check_dir() )
		return false;

	Glib::ustring cfgfile = Glib::build_filename(Glib::ustring(g_get_user_config_dir()), Glib::ustring("nitrogen"));
	cfgfile = Glib::build_filename(cfgfile, Glib::ustring("nitrogen.cfg"));

    Glib::KeyFile kf;

    kf.set_integer("geometry", "posx", m_posx);
    kf.set_integer("geometry", "posy", m_posy);
    kf.set_integer("geometry", "sizex", m_sizex);
    kf.set_integer("geometry", "sizey", m_sizey);

    if (m_display_mode == ICON)
        kf.set_string("nitrogen", "view",  Glib::ustring("icon"));
    else if (m_display_mode == LIST)
        kf.set_string("nitrogen", "view",  Glib::ustring("list"));

	switch (m_sort_mode) {
	case Thumbview::SORT_ALPHA:
		kf.set_string("nitrogen", "sort", Glib::ustring("alpha"));
		break;
	case Thumbview::SORT_RALPHA:
		kf.set_string("nitrogen", "sort", Glib::ustring("ralpha"));
		break;
	case Thumbview::SORT_TIME:
		kf.set_string("nitrogen", "sort", Glib::ustring("time"));
		break;
	case Thumbview::SORT_RTIME:
		kf.set_string("nitrogen", "sort", Glib::ustring("rtime"));
		break;
	}

    kf.set_boolean("nitrogen", "icon_caps", m_icon_captions);

    kf.set_string_list("nitrogen", "dirs", m_vec_dirs);

    if (g_file_set_contents(cfgfile.c_str(), kf.to_data().c_str(), -1, NULL) == FALSE)
        return false;

    return true;
}

/**
 * Loads common configuration params from the configuration file.
 */
bool Config::load_cfg()
{
	if ( ! Config::check_dir() )
		return false;

	Glib::ustring cfgfile = Glib::build_filename(Glib::ustring(g_get_user_config_dir()), Glib::ustring("nitrogen"));
	cfgfile = Glib::build_filename(cfgfile, Glib::ustring("nitrogen.cfg"));

    if (!Glib::file_test(cfgfile, Glib::FILE_TEST_EXISTS))
        return false;

    // TODO: use load_from_data_dirs?    
    Glib::KeyFile kf;
    if (!kf.load_from_file(cfgfile))
        return false;

    if (kf.has_key("geometry", "posx"))     m_posx = kf.get_integer("geometry", "posx");
    if (kf.has_key("geometry", "posy"))     m_posy = kf.get_integer("geometry", "posy");
    if (kf.has_key("geometry", "sizex"))    m_sizex = kf.get_integer("geometry", "sizex");
    if (kf.has_key("geometry", "sizey"))    m_sizey = kf.get_integer("geometry", "sizey");

    if (kf.has_key("nitrogen", "view"))
    {
        Glib::ustring mode = kf.get_string("nitrogen", "view");
        if (mode == Glib::ustring("icon"))
            m_display_mode = ICON;
        else if (mode == Glib::ustring("list"))
            m_display_mode = LIST;
    }

	if(kf.has_key("nitrogen", "sort")) {
		Glib::ustring mode = kf.get_string("nitrogen", "sort");
		if (mode == Glib::ustring("alpha")) m_sort_mode = Thumbview::SORT_ALPHA;
		else if (mode == Glib::ustring("ralpha")) m_sort_mode = Thumbview::SORT_RALPHA;
		else if (mode == Glib::ustring("time")) m_sort_mode = Thumbview::SORT_TIME;
		else m_sort_mode = Thumbview::SORT_RTIME;
	}

    if (kf.has_key("nitrogen", "dirs"))         m_vec_dirs = kf.get_string_list("nitrogen", "dirs");
    if (kf.has_key("nitrogen", "icon_caps"))    m_icon_captions = kf.get_boolean("nitrogen", "icon_caps");

    return true;
}

VecStrs Config::get_dirs()
{
    return m_vec_dirs;
}

void Config::set_dirs(const VecStrs& new_dirs)
{
    m_vec_dirs = new_dirs;
}

/**
 * Adds a directory to the list of directories.
 *
 * Does not add if the passed dir already is in the list of dirs.
 * Returns a boolean to indicate if this call resulted in an add.
 * If it returns false, it didn't add a new one becuase it already 
 * existed.
 */
bool Config::add_dir(const std::string& dir)
{
    VecStrs::iterator i = std::find(m_vec_dirs.begin(), m_vec_dirs.end(), dir);
    if (i == m_vec_dirs.end())
    {
        m_vec_dirs.push_back(std::string(dir));
        return true;
    }

    return false;
}

/**
 * Removes the given directory from the list of directories.
 *
 * Returns true if it was able to find and remove it. Otherwise, returns
 * false.
 */
bool Config::rm_dir(const std::string& dir)
{
    VecStrs::iterator i = std::find(m_vec_dirs.begin(), m_vec_dirs.end(), dir);
    if (i != m_vec_dirs.end())
    {
        m_vec_dirs.erase(i);
        return true;
    }

    return false;
}


