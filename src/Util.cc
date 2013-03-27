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

#include "Util.h"
#include "SetBG.h"
#include "Config.h"
#include <iostream>
#include <stdio.h>
#include "gcs-i18n.h"
#include <glib/gprintf.h>
#include "NWindow.h"

namespace Util {

// parafarmance testing / bug finding
// http://primates.ximian.com/~federico/news-2006-03.html#09
//
// this functiona always does something.  If the enable-debug flag is
// on when configured, this prints out to stdout.  if not, it tries to 
// access() a string which is useful for the link above.
void program_log (const char *format, ...)
{
	va_list args;
    char *formatted, *str;

    va_start (args, format);
    formatted = g_strdup_vprintf (format, args);
    va_end (args);

    str = g_strdup_printf ("MARK: %s: %s", g_get_prgname(), formatted);
    g_free (formatted);

#ifdef DEBUG
    g_printf("%s\n", str);
#else
	access (str, F_OK);
#endif    
    g_free (str);
} 

// Converts a relative path to an absolute path.
// Probably works best if the path doesn't start with a '/' :)
// XXX: There must be a better way to do this, haha.
Glib::ustring path_to_abs_path(Glib::ustring path) {
	unsigned parents = 0;
	Glib::ustring cwd = Glib::get_current_dir();
	Glib::ustring::size_type pos;

	// find out how far up we have to go.
	while( (pos = path.find("../", 0)) !=Glib::ustring::npos ) {
		path.erase(0, 3);
		parents++;
	}
	
	// now erase that many directories from the path to the current
	// directory.
	
	while( parents ) {
		if ( (pos = cwd.rfind ('/')) != Glib::ustring::npos) {
			// remove this directory from the path
			cwd.erase (pos);
		}
		parents--;
	}
	
	return cwd + '/' + path;
}

/**
 * Creates an instance of an ArgParser with all options set up.
 *
 * @returns		An ArgParser instance.
 */
ArgParser* create_arg_parser() {

    ArgParser* parser = new ArgParser();
    parser->register_option("restore", _("Restore saved backgrounds"));
    parser->register_option("no-recurse", _("Do not recurse into subdirectories"));
    parser->register_option("sort", _("How to sort the backgrounds. Valid options are:\n\t\t\t* alpha, for alphanumeric sort\n\t\t\t* ralpha, for reverse alphanumeric sort\n\t\t\t* time, for last modified time sort (oldest first)\n\t\t\t* rtime, for reverse last modified time sort (newest first)"), true);
    parser->register_option("set-color", _("background color in hex, #000000 by default"), true);
    parser->register_option("head", _("Select xinerama/multihead display in GUI, 0..n, -1 for full"), true);
    parser->register_option("force-setter", _("Force setter engine: xwindows, xinerama, gnome"), true);

    // command line set modes
    Glib::ustring openp(" (");
    Glib::ustring closep(")");
    parser->register_option("set-scaled", _("Sets the background to the given file") + openp + _("scaled") + closep);
    parser->register_option("set-tiled", _("Sets the background to the given file") + openp + _("tiled") + closep);
    parser->register_option("set-auto", _("Sets the background to the given file") + openp + _("auto") + closep);
    parser->register_option("set-centered", _("Sets the background to the given file") + openp + _("centered") + closep);
    parser->register_option("set-zoom", _("Sets the background to the given file") + openp + _("zoom") + closep);
    parser->register_option("set-zoom-fill", _("Sets the background to the given file") + openp + _("zoom-fill") + closep);
    parser->register_option("save", _("Saves the background permanently"));

    std::vector<std::string> vecsetopts;
    vecsetopts.push_back("set-scaled");
    vecsetopts.push_back("set-tiled");
    vecsetopts.push_back("set-auto");
    vecsetopts.push_back("set-centered");
    vecsetopts.push_back("set-zoom");
    vecsetopts.push_back("set-zoom-fill");

    parser->make_exclusive(vecsetopts);

    return parser;
}

/**
 * Prepares the inputted start dir to the conventions needed throughout the program.
 *
 * Changes a relative dir to a absolute dir, trims terminating slash.
 */
std::string fix_start_dir(std::string startdir) {

	// trims the terminating '/'. if it exists
	if ( startdir[startdir.length()-1] == '/' ) {
		startdir.resize(startdir.length()-1);
	}
	
	// if this is not an absolute path, make it one.
	if ( !Glib::path_is_absolute(startdir) ) {
		startdir = Util::path_to_abs_path(startdir);
	}

	return startdir;
}

/**
 * Determines if the passed display is one that is currently seen by the program.
 *
 * Displays are passed in here to determine if they need to be shown as the "current
 * background".
 */
bool is_display_relevant(Gtk::Window* window, Glib::ustring display)
{
    // cast window to an NWindow.  we have to do this to avoid a circular dep
    NWindow *nwindow = dynamic_cast<NWindow*>(window);
    return (nwindow->map_displays.find(display) != nwindow->map_displays.end());
}

/**
 * Makes a string for the UI to indicate it is the currently selected background.
 *
 * Checks to make sure the display is relevant with is_display_relevant(). If it is
 * not relevent, it simply returns the filename.
 *
 * ex: "filename\nCurrently set background for Screen 1"
 */
Glib::ustring make_current_set_string(Gtk::Window* window, Glib::ustring filename, Glib::ustring display)
{
    // cast window to an NWindow.  we have to do this to avoid a circular dep
    NWindow *nwindow = dynamic_cast<NWindow*>(window);
    
    Glib::ustring shortfile(filename, filename.rfind("/")+1);
    if (!is_display_relevant(window, display))
        return shortfile;

    std::ostringstream ostr;
    ostr << shortfile << "\n\n" << "<i>" << _("Currently set background");

    if (nwindow->map_displays.size() > 1)
        ostr << " " << _("for") << " " << nwindow->map_displays[display];
   
    ostr << "</i>";

    return ostr.str();
}

}
