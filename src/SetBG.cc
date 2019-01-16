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

#include "SetBG.h"
#include "main.h"
#include <X11/Xatom.h>
#include "gcs-i18n.h"
#include "Util.h"
#include "Config.h"
#include <algorithm>
#include <functional>
#include <iomanip>
#include <sstream>

using namespace Util;

/**
 * Factory method to produce a concrete SetBG derived class.
 * You are responsible for freeing the result.
 */
SetBG* SetBG::get_bg_setter()
{
    Glib::RefPtr<Gdk::Display> dpy = Gdk::DisplayManager::get()->get_default_display();

    SetBG* setter = NULL;
    SetBG::RootWindowData wd = SetBG::get_root_window_data(dpy);

    Util::program_log("root window type %d\n", wd.type);

    switch (wd.type) {
        case SetBG::NAUTILUS:
            setter = new SetBGGnome();
            break;
        case SetBG::NEMO:
            setter = new SetBGNemo();
            break;
#ifdef USE_XINERAMA
        case SetBG::XINERAMA:
            XineramaScreenInfo *xinerama_info;
            gint xinerama_num_screens;

            xinerama_info = XineramaQueryScreens(GDK_DISPLAY_XDISPLAY(dpy->gobj()), &xinerama_num_screens);

            setter = new SetBGXinerama();
            ((SetBGXinerama*)setter)->set_xinerama_info(xinerama_info, xinerama_num_screens);
            break;
#endif
        case SetBG::PCMANFM:
            setter = new SetBGPcmanfm();
            break;
        case SetBG::XFCE:
            setter = new SetBGXFCE();
            break;
        case SetBG::DEFAULT:
        default:
            setter = new SetBGXWindows();
            break;
    };

    return setter;
}

/**
 * Handles a BadWindow error in check_window_type.
 *
 * This is rather hax, but not sure how else to handle it.
 */
int SetBG::handle_x_errors(Display *display, XErrorEvent *error)
{
    if (error->error_code == BadWindow)
        return 0;

    std::cerr << "SetBG::handle_x_errors received an error it cannot handle!\n";
    std::cerr << "\tcode: " << error->error_code << "\n";
    std::cerr << "\nWe must abort!";
    exit(1);

    return 1;
}

/**
 * Recursive function to find windows with _NET_WM_WINDOW_TYPE set to
 * _NET_WM_WINDOW_TYPE_DESKTOP that we understand how to deal with.
 *
 * Returns a vector of RootWindowData objects with all found desktop windows,
 * ignoring known non-root-windows.
 */
std::vector<SetBG::RootWindowData> SetBG::find_desktop_windows(Display *xdisp, Window curwindow) {
    Window rr, pr;
    Window *children;
    unsigned int numchildren;
    Atom ret_type;
    int ret_format;
    unsigned long ret_items, ret_bytesleft;
    Atom *prop_return;

    std::vector<SetBG::RootWindowData> retVec;

    // search current window for atom
    Atom propatom = XInternAtom(xdisp, "_NET_WM_WINDOW_TYPE", False);
    Atom propval  = XInternAtom(xdisp, "_NET_WM_WINDOW_TYPE_DESKTOP", False);

    XGetWindowProperty(xdisp, curwindow, propatom, 0L, 32L, False,
            XA_ATOM, &ret_type, &ret_format, &ret_items, &ret_bytesleft,
            (unsigned char**) &prop_return);

    if (ret_type == XA_ATOM && ret_format == 32 && ret_items == 1) {
        if (prop_return[0] == propval) {
            XFree(prop_return);

            SetBG::RootWindowData bgRw = check_window_type(xdisp, curwindow);

            if (bgRw.type != SetBG::IGNORE)
                retVec.push_back(bgRw);
        }
    } else {
        // mutter now just sets a single property, WM_NAME = "mutter guard window", so detect that
        Atom nameatom = XInternAtom(xdisp, "WM_NAME", False);
        XTextProperty tprop;

        gchar **list;
        gint num;

        XErrorHandler old_x_error_handler = XSetErrorHandler(SetBG::handle_x_errors);
        bool bGotText = XGetTextProperty(xdisp, curwindow, &tprop, nameatom);
        XSetErrorHandler(old_x_error_handler);

        if (bGotText && tprop.nitems)
        {
            if (XTextPropertyToStringList(&tprop, &list, &num))
            {
                if (num == 1 && std::string(list[0]) == std::string("mutter guard window")) {

                    SetBG::RootWindowData bgMutter(curwindow, SetBG::NAUTILUS, std::string(""));
                    retVec.push_back(bgMutter);
                }
                XFreeStringList(list);
            }
            XFree(tprop.value);
        }
    }

    // iterate all children of current window
    XQueryTree(xdisp, curwindow, &rr, &pr, &children, &numchildren);
    for (int i = 0; i < numchildren; i++) {
        std::vector<SetBG::RootWindowData> childVec = find_desktop_windows(xdisp, children[i]);
        retVec.insert(retVec.end(), childVec.begin(), childVec.end());
    }
    XFree(children);

    return retVec;
}

/**
 * Determines the root window type that will be used to generate a setter.
 *
 * Returns a RootWindowData structure with the proper information that should be used
 * to create a Setter instance. It:
 * - Checks for Nautilus/Mutter Atoms
 * - Launches recursive method to iterate windows and find possible desktop matches
 * - Decides which most corresponds to the user's setup and returns it
 */
SetBG::RootWindowData SetBG::get_root_window_data(Glib::RefPtr<Gdk::Display> display) {
	GdkAtom type;
    gint format;
    gint length;
	guchar *data;
    gboolean ret = FALSE;
    Glib::RefPtr<Gdk::Window> rootwin = display->get_default_screen()->get_root_window();
    Display *xdisp = GDK_DISPLAY_XDISPLAY(rootwin->get_display()->gobj());
    Window xrootwin = (Window)GDK_WINDOW_XID(rootwin->gobj());
    guint wid = 0;

    ret = gdk_property_get(rootwin->gobj(),
                           gdk_atom_intern("NAUTILUS_DESKTOP_WINDOW_ID", FALSE),
                           gdk_atom_intern("WINDOW", FALSE),
                           0L,
                           4L, /* length of a window is 32bits*/
                           FALSE, &type, &format, &length, &data);

    if (ret) {
        wid = *(guint*)data;
        g_free(data);

        // xfce also sets NAUTILUS_DESKTOP_WINDOW_ID, check for XFCE_DESKTOP_WINDOW
        ret = gdk_property_get(rootwin->gobj(),
                               gdk_atom_intern("XFCE_DESKTOP_WINDOW", FALSE),
                               gdk_atom_intern("WINDOW", FALSE),
                               0L,
                               4L, /* length of a window is 32bits*/
                               FALSE, &type, &format, &length, &data);

        if (ret) {
            wid = *(guint*)data;
            g_free(data);

            return SetBG::RootWindowData((Window)wid, SetBG::XFCE, "XFCE_DESKTOP_WINDOW");
        }

        return SetBG::RootWindowData((Window)wid, SetBG::NAUTILUS, "NAUTILUS_DESKTOP_WINDOW_ID");
    } else {
        // check for mutter atoms on root window
        ret = gdk_property_get(rootwin->gobj(),
                gdk_atom_intern("_MUTTER_SENTINEL", FALSE),
                gdk_atom_intern("CARDINAL", FALSE),
                0L,
                4L,
                FALSE, &type, &format, &length, &data);

        if (ret) {
            g_free(data);
            return SetBG::RootWindowData(xrootwin, SetBG::NAUTILUS, std::string("_MUTTER_SENTINEL"));
        } else {
            // newer nautilus and nemo don't leave a hint on the root window (whyyyy)
            // now we have to search for it!
            Window xwin = DefaultRootWindow(xdisp);
            std::vector<SetBG::RootWindowData> rootVec = find_desktop_windows(xdisp, xwin);

            if (rootVec.size() > 1) {
                std::vector<SetBG::RootWindowData>::const_iterator selIter = rootVec.end();

                std::cerr << "WARNING: More than one Desktop window found:\n";

                for (std::vector<SetBG::RootWindowData>::const_iterator i = rootVec.begin(); i != rootVec.end(); i++) {

                    std::string selStr("    ");
                    if (i->type != SetBG::UNKNOWN && selIter == rootVec.end()) {
                        selIter = i;
                        selStr = std::string("**  ");
                    }

                    std::cerr << selStr << std::setw(6) << "0x" << std::setw(8) << std::hex << i->window << std::setw(4) << i->type << std::setw(12) << i->wm_class << "\n";
                }

                if (selIter != rootVec.end())
                    return *selIter;

                // @TODO: SET FLAG SOMEWHERE TO INDICATE ISSUE
            } else if (rootVec.size() == 1) {
                return rootVec[0];
            } else {
                // no finds, fall through to default
            }
        }
    }

#ifdef USE_XINERAMA
    // determine if xinerama is in play
    if (XineramaIsActive(xdisp))
        return SetBG::RootWindowData(xrootwin, SetBG::XINERAMA, std::string("default"));
#endif

    return SetBG::RootWindowData(xrootwin, SetBG::DEFAULT, std::string("default"));
}

/**
 * Determines if the passed in Window is a window type that marks a false root window.
 *
 * Returns a RootWindowData always, if window is not anything we recognize, the type will
 * be set to UNKNOWN.
 *
 * Certain applications like conky with own_window and own_window_type='desktop'
 * are valid and should be ignored. This method will log anything
 * found.
 */
SetBG::RootWindowData SetBG::check_window_type(Display *display, Window window)
{
    SetBG::RootWindowData retval(window);
    Atom propatom = XInternAtom(display, "WM_CLASS", FALSE);

    XTextProperty tprop;

    gchar **list;
    gint num;

    XErrorHandler old_x_error_handler = XSetErrorHandler(SetBG::handle_x_errors);
    bool bGotText = XGetTextProperty(display, window, &tprop, propatom);
    XSetErrorHandler(old_x_error_handler);

    if (bGotText && tprop.nitems)
    {
        if (XTextPropertyToStringList(&tprop, &list, &num))
        {
            // expect 2 strings here (XLib tells us there are 3)
            if (num != 3) {
                // @TODO: warn? does this even happen?
                retval.type = SetBG::DEFAULT;
            }
            else
            {
                std::string strclass = std::string(list[1]);
                retval.wm_class = strclass;

                if (strclass == std::string("Xfdesktop")) retval.type = SetBG::XFCE;     else
                if (strclass == std::string("Nautilus"))  retval.type = SetBG::NAUTILUS; else
                if (strclass == std::string("Nemo"))      retval.type = SetBG::NEMO;     else
                if (strclass == std::string("Pcmanfm"))   retval.type = SetBG::PCMANFM;  else
                if (strclass == std::string("Conky") || strclass == std::string("conky"))
                    retval.type = SetBG::IGNORE;   else        // discard Conky!
                if (strclass == std::string("Easystroke"))
                    retval.type = SetBG::IGNORE;   else
                if (strclass == std::string("Tilda"))
                    retval.type = SetBG::IGNORE;   else
                {
                    std::cerr << "UNKNOWN ROOT WINDOW TYPE DETECTED (" << strclass << "), please file a bug\n";
                    retval.type = SetBG::UNKNOWN;
                }
            }
            XFreeStringList(list);
        }
        XFree(tprop.value);
    }

    return retval;
}

/**
 * Handles SET_SCALE mode.
 *
 * @param	orig	The original pixbuf
 * @param	winw	Width of the window
 * @param	winh	Height of the window
 * @param	bgcolor	Background color (Unused)
 */
Glib::RefPtr<Gdk::Pixbuf> SetBG::make_scale(const Glib::RefPtr<Gdk::Pixbuf> orig, const gint winw, const gint winh, Gdk::Color bgcolor) {
	Glib::RefPtr<Gdk::Pixbuf> retval = orig->scale_simple(winw, winh, Gdk::INTERP_BILINEAR);
	return retval;
}

/**
 * Handles SET_TILE mode.
 *
 * @param	orig	The original pixbuf
 * @param	winw	Width of the window
 * @param	winh	Height of the window
 * @param	bgcolor	Background color (Unused)
 */
Glib::RefPtr<Gdk::Pixbuf> SetBG::make_tile(const Glib::RefPtr<Gdk::Pixbuf> orig, const gint winw, const gint winh, Gdk::Color bgcolor) {
	// copy and resize (mainly just resize :)
	Glib::RefPtr<Gdk::Pixbuf> retval = orig->scale_simple(winw, winh, Gdk::INTERP_NEAREST);

	int orig_width = orig->get_width();
	int orig_height = orig->get_height();

	unsigned count = 0;

	// copy across horizontally first
	unsigned iterations = (unsigned)ceil((double)winw / (double)orig_width);
	for (count = 0; count < iterations; count++) {
		orig->copy_area(0, 0, ((count + 1) * orig_width) > winw ? orig_width - (((count+1) * orig_width) - winw) : orig_width, orig_height, retval, count * orig_width, 0);
	}

	// now vertically
	iterations = (unsigned)ceil((double)winh / (double)orig_height);
	// start at 1 because the first real (0) iteration is already done from before (it's the source of our copy!)
	for (count = 1; count < iterations; count++) {
		retval->copy_area(0, 0, winw, ((count+ 1)*orig_height) > winh ? orig_height - (((count+1) * orig_height) - winh) : orig_height, retval, 0, count * orig_height);
	}

	return retval;
}

/**
 * Handles SET_CENTER mode.  Crops if needed.
 *
 * @param	orig	The original pixbuf
 * @param	winw	Width of the window
 * @param	winh	Height of the window
 * @param	bgcolor	Background color
 */
Glib::RefPtr<Gdk::Pixbuf> SetBG::make_center(const Glib::RefPtr<Gdk::Pixbuf> orig, const gint winw, const gint winh, Gdk::Color bgcolor) {

	Glib::RefPtr<Gdk::Pixbuf> retval = Gdk::Pixbuf::create(	orig->get_colorspace(),
															orig->get_has_alpha(),
															orig->get_bits_per_sample(),
															winw,
															winh);

	// use passed bg color
	retval->fill(GdkColorToUint32(bgcolor));

	int destx = (winw - orig->get_width()) >> 1;
	int desty = (winh - orig->get_height()) >> 1;
	int srcx = 0;
	int srcy = 0;
	int cpyw = orig->get_width();
	int cpyh = orig->get_height();

	if ( orig->get_width() > winw ) {
		srcx = (orig->get_width()-winw) >> 1;
		destx = 0;
		cpyw = winw;
	}
	if ( orig->get_height() > winh) {
		srcy = (orig->get_height()-winh) >> 1;
		desty = 0;
		cpyh = winh;
	}

	orig->copy_area(srcx, srcy, cpyw, cpyh, retval, destx, desty);

	return retval;
}

/**
 * Handles SET_ZOOM mode.
 *
 * @param	orig	The original pixbuf
 * @param	winw	Width of the window
 * @param	winh	Height of the window
 * @param	bgcolor	Background color
 */
Glib::RefPtr<Gdk::Pixbuf> SetBG::make_zoom(const Glib::RefPtr<Gdk::Pixbuf> orig, const gint winw, const gint winh, Gdk::Color bgcolor) {

	int x, y, resx, resy;
	x = y = 0;

	// depends on bigger side
	unsigned orig_w = orig->get_width();
	unsigned orig_h = orig->get_height();
	// the second term (after the &&) is needed to ensure that the new height
	// does not exceed the root window height
	if ( orig_w > orig_h && ((float)orig_w / (float)orig_h) > ((float)winw / (float)winh)) {
		resx = winw;
		resy = (int)(((float)(orig->get_height()*resx))/(float)orig->get_width());
		x = 0;
		y = (winh - resy) >> 1;

	} else {
		resy = winh;
		resx = (int)(((float)(orig->get_width()*resy))/(float)orig->get_height());
		y = 0;
		x = (winw - resx) >> 1;

	}

	// fix to make sure we can make it
	if ( resx > winw )
		resx = winw;
	if ( resy > winh )
		resy = winh;
	if ( x < 0 )
		x = 0;
	if ( y < 0 )
		y = 0;

	Glib::RefPtr<Gdk::Pixbuf> tmp = orig->scale_simple(resx, resy,
		Gdk::INTERP_BILINEAR);
	Glib::RefPtr<Gdk::Pixbuf> retval = Gdk::Pixbuf::create(
		orig->get_colorspace(), orig->get_has_alpha(),
		orig->get_bits_per_sample(), winw, winh);

	// use passed bg color
	retval->fill(GdkColorToUint32(bgcolor));

	// copy it in
	tmp->copy_area(0, 0, tmp->get_width(), tmp->get_height(), retval, x, y);

	return retval;
}

/**
 * Handles SET_ZOOM_FILL mode.
 *
 * @param	orig	The original pixbuf
 * @param	winw	Width of the window
 * @param	winh	Height of the window
 * @param	bgcolor	Background color
 */
Glib::RefPtr<Gdk::Pixbuf> SetBG::make_zoom_fill(const Glib::RefPtr<Gdk::Pixbuf> orig, const gint winw, const gint winh, Gdk::Color bgcolor) {

	int x, y, w, h;

	// depends on bigger side
	unsigned orig_w = orig->get_width();
	unsigned orig_h = orig->get_height();

    int dw = winw - orig_w;
    int dh = winh - orig_h;

    // what if we expand it to fit the screen width?
    x = 0;
    w = winw;
    h = winw * orig_h / orig_w;
    y = (h - winh) / 2;

    if (!(h >= winh)) {
        // the image isn't tall enough that way!
        // expand it to fit the screen height
        y = 0;
        w = winh * orig_w / orig_h;
        h = winh;
        x = (w - winw) / 2;
    }

	Glib::RefPtr<Gdk::Pixbuf> tmp = orig->scale_simple(w, h,
		Gdk::INTERP_BILINEAR);
	Glib::RefPtr<Gdk::Pixbuf> retval = Gdk::Pixbuf::create(
		orig->get_colorspace(), orig->get_has_alpha(),
		orig->get_bits_per_sample(), winw, winh);

	// use passed bg color
	retval->fill(GdkColorToUint32(bgcolor));

	// copy it in
	tmp->copy_area(x, y, winw, winh, retval, 0, 0);

	return retval;
}

/**
 * Utility function to convert a mode (an enum) to a string.
 */
Glib::ustring SetBG::mode_to_string( const SetMode mode ) {

	Glib::ustring ret;

	switch ( mode ) {
		case SET_SCALE:
			ret = Glib::ustring(_("Scale"));
			break;
		case SET_CENTER:
			ret = Glib::ustring(_("Center"));
			break;
		case SET_TILE:
			ret = Glib::ustring(_("Tile"));
			break;
		case SET_ZOOM:
			ret = Glib::ustring(_("Zoom"));
			break;
		case SET_ZOOM_FILL:
			ret = Glib::ustring(_("ZoomFill"));
			break;
        case SET_AUTO:
            ret = Glib::ustring(_("Auto"));
            break;
	};

	return ret;
}

/**
 * Utility function to translate to SetMode from a string.  Meant to be used from the values
 * produced in the above function.
 */
SetBG::SetMode SetBG::string_to_mode( const Glib::ustring str ) {

	if ( str == Glib::ustring(_("Scale")) )
		return SetBG::SET_SCALE;
	else if ( str == Glib::ustring(_("Center")) )
		return SetBG::SET_CENTER;
	else if ( str == Glib::ustring(_("Tile")) )
		return SetBG::SET_TILE;
	else if ( str == Glib::ustring(_("Zoom")) )
		return SetBG::SET_ZOOM;
	else if ( str == Glib::ustring(_("ZoomFill")) )
		return SetBG::SET_ZOOM_FILL;
    else if ( str == Glib::ustring(_("Auto")) )
        return SetBG::SET_AUTO;

	// shouldn't get here
	return SetBG::SET_AUTO;
}

/**
 * Transforms the passed Gdk::Color object to a guint32.  Used for filling a Gdk::Pixbuf.
 *
 * @param	col		The color to translate.
 * @return			A guint32
 */
guint32 SetBG::GdkColorToUint32(const Gdk::Color col)
{
	guint32 ret = 0x00000000;
	ret |= ((unsigned int)(col.get_red_p() * 255) << 24);
	ret |= ((unsigned int)(col.get_green_p() * 255) << 16);
	ret |= ((unsigned int)(col.get_blue_p() * 255) << 8);

	// alpha should always be full (this caused ticket 4)
	ret |= 255;

	return ret;
}

/**
 * Determines the best set mode for the pixbuf based on its size relative to
 * the window size.
 *
 * @param   pixbuf  The loaded pixbuf from the file, before it has been sized.
 * @param   width   The width of the root window.
 * @param   height  The height of the root window.
 */
SetBG::SetMode SetBG::get_real_mode(const Glib::RefPtr<Gdk::Pixbuf> pixbuf, const gint width, const gint height)
{
    SetBG::SetMode mode = SetBG::SET_ZOOM;
    float ratio = ((float)pixbuf->get_width()) / ((float)pixbuf->get_height());

    float f2t = 1.333f;
    float f2f = 1.25f;

    if (fabsf(ratio - f2t) < 0.001)
        mode = SetBG::SET_SCALE;
    else if (fabsf(ratio - f2f) < 0.001)
        mode = SetBG::SET_SCALE;
    else if (ratio == 1.0 && pixbuf->get_width() <= 640)
        mode = SetBG::SET_TILE;
    else if (pixbuf->get_width() <= width && pixbuf->get_height() <= height)
        mode = SetBG::SET_CENTER;

    return mode;
}

void SetBG::restore_bgs()
{
    std::vector<Glib::ustring> displist;
	Glib::ustring file, display;
	SetBG::SetMode mode;
	Gdk::Color bgcolor;

    // saving of pixmaps is for interactive only
    disable_pixmap_save();

	Config *cfg = Config::get_instance();

    if (cfg->get_bg_groups(displist) == false) {
        // @TODO exception??
        std::cerr << _("Could not get bg groups from config file.") << std::endl;
        return;
    }

    std::vector<Glib::ustring> filtered_list;
    std::vector<Glib::ustring>::iterator i;

    Glib::ustring prefix = this->get_prefix();

    for (i = displist.begin(); i != displist.end(); i++)
        if ((*i).compare(0, prefix.length(), prefix) == 0)
            filtered_list.push_back((*i));

    // do we have a "full screen" item set?
    Glib::ustring fullscreen = this->get_fullscreen_key();

    i = std::find_if(filtered_list.begin(), filtered_list.end(), std::bind2nd(std::equal_to<Glib::ustring>(), fullscreen));

    if (i == filtered_list.end()) {
        for (i = filtered_list.begin(); i != filtered_list.end(); i++)
        {
            if (cfg->get_bg(*i, file, mode, bgcolor)) {
                this->set_bg((*i), file, mode, bgcolor);
            } else {
                std::cerr << _("ERROR") << std::endl;
            }
        }
    } else {
        if (cfg->get_bg(*i, file, mode, bgcolor)) {
            this->set_bg((*i), file, mode, bgcolor);
        } else {
            std::cerr << _("ERROR") << std::endl;
        }
    }
}

/**
 * Returns a Gdk::Display object for use with any of the set_bg derivations.
 */
Glib::RefPtr<Gdk::Display> SetBG::get_display(const Glib::ustring& disp)
{
    Glib::RefPtr<Gdk::Display> _display = Gdk::Display::open(Gdk::DisplayManager::get()->get_default_display()->get_name());
	if (!_display) {
		std::cerr << _("Could not open display") << "\n";
		throw 1;
	}

    return _display;
}

/**
 * Base driver for set_bg in derived classes.
 *
 * @param	disp	The display to set it on.  If "", uses the default display, and passes is out by reference.
 * @param	file	The file to set the bg to
 * @param	mode	Stretch, tile, center, bestfit
 * @param	bgcolor	The background color for modes that do not cover entire portions
 * @return			If it went smoothly
 */
bool SetBG::set_bg(Glib::ustring &disp, Glib::ustring file, SetMode mode, Gdk::Color bgcolor)
{
    gint winx,winy,winw,winh,wind;
	gint tarx, tary, tarw, tarh;
    Glib::RefPtr<Gdk::Display> _display = this->get_display(disp);
    Glib::RefPtr<Gdk::Screen> screen;
    Glib::RefPtr<Gdk::Window> window;
    Glib::RefPtr<Gdk::GC> gc_;
    Glib::RefPtr<Gdk::Colormap> colormap;
    Glib::RefPtr<Gdk::Pixbuf> pixbuf, outpixbuf;
    Glib::RefPtr<Gdk::Pixmap> pixmap;
    Display *xdisp = GDK_DISPLAY_XDISPLAY(_display->gobj());
    Window xwin = DefaultRootWindow(xdisp);
    Atom prop_root, prop_esetroot, type;
    int format;
    unsigned long length, after;
    unsigned char *data_root, *data_esetroot;

    // get the screen
    screen = _display->get_default_screen();

    // get window stuff
    window = screen->get_root_window();
    window->get_geometry(winx, winy, winw, winh, wind);

    // get target dimensions
    if (!get_target_dimensions(disp, winx, winy, winw, winh, tarx, tary, tarw, tarh))
        return false;

    // create gc and colormap
    gc_ = Gdk::GC::create(window);
    colormap = screen->get_default_colormap();

    // allocate bg color
    colormap->alloc_color(bgcolor, false, true);

    // get or create pixmap - for xinerama, may use existing one to merge
    // this will delete the old pixmap if necessary
    // also sets shutdown mode if necessary
    pixmap = this->get_or_create_pixmap(disp, _display, window, winw, winh, wind, colormap);

    // get our pixbuf from the file
    try {
        pixbuf = Gdk::Pixbuf::create_from_file(file);
    } catch (const Glib::FileError& e) {
        std::cerr << _("ERROR: Could not load file in bg set") << ": " << e.what() << "\n";
        return false;
    }

    outpixbuf = this->make_resized_pixbuf(pixbuf, mode, bgcolor, tarw, tarh);

    // render it to the pixmap
	pixmap->draw_pixbuf(gc_, outpixbuf, 0, 0, tarx, tary, tarw, tarh, Gdk::RGB_DITHER_NONE, 0, 0);

    // set it via XLib
    Pixmap xpm = GDK_PIXMAP_XID(pixmap->gobj());

    set_current_pixmap(_display, &xpm);

    // cleanup
	gc_.clear();

    // close down display
    _display->close();

    return true;
}

/**
 * Sets the target dimensions based on window dimensions and display.
 *
 * For this base version, simply copies the window dimensions to the target
 * dimensions.
 */
bool SetBG::get_target_dimensions(Glib::ustring& disp, gint winx, gint winy, gint winw, gint winh, gint& tarx, gint& tary, gint& tarw, gint& tarh)
{
    tarx = winx;
    tary = winy;
    tarw = winw;
    tarh = winh;

    return true;
}

/**
 * Creates a resized pixbuf given the mode, bgcolor, and target size.
 */
Glib::RefPtr<Gdk::Pixbuf> SetBG::make_resized_pixbuf(Glib::RefPtr<Gdk::Pixbuf> pixbuf, SetBG::SetMode mode, Gdk::Color bgcolor, gint tarw, gint tarh)
{
    Glib::RefPtr<Gdk::Pixbuf> outpixbuf;

    // apply the bg color to pixbuf here, because every make_ method would
    // have to do it anyway.
    pixbuf = pixbuf->composite_color_simple(pixbuf->get_width(),
            pixbuf->get_height(), Gdk::INTERP_NEAREST, 255, 1, bgcolor.get_pixel(),
            bgcolor.get_pixel());

    // if automatic, figure out what mode we really want
    if (mode == SetBG::SET_AUTO)
        mode = SetBG::get_real_mode(pixbuf, tarw, tarh);

    switch(mode) {
        case SetBG::SET_SCALE:
            outpixbuf = SetBG::make_scale(pixbuf, tarw, tarh, bgcolor);
            break;

        case SetBG::SET_TILE:
            outpixbuf = SetBG::make_tile(pixbuf, tarw, tarh, bgcolor);
            break;

        case SetBG::SET_CENTER:
            outpixbuf = SetBG::make_center(pixbuf, tarw, tarh, bgcolor);
            break;

        case SetBG::SET_ZOOM:
            outpixbuf = SetBG::make_zoom(pixbuf, tarw, tarh, bgcolor);
            break;

        case SetBG::SET_ZOOM_FILL:
            outpixbuf = SetBG::make_zoom_fill(pixbuf, tarw, tarh, bgcolor);
            break;

        default:
            std::cerr << _("Unknown mode for saved bg") << std::endl;
            // @TODO: raise exception
            //return false;
    };

    return outpixbuf;
}

/**
 * Returns the Pixmap to use as the permanent background.
 *
 * This method will either wrap and return the existing pixmap to be reused,
 * or create a new one if one doesn't exist or the dimensions are incorrect.
 *
 * It will delete the old pixmap if appropriate and set the proper close down
 * mode on the X display.
 */
Glib::RefPtr<Gdk::Pixmap> SetBG::get_or_create_pixmap(Glib::ustring disp, Glib::RefPtr<Gdk::Display> _display, Glib::RefPtr<Gdk::Window> window, gint winw, gint winh, gint wind, Glib::RefPtr<Gdk::Colormap> colormap)
{
    Glib::RefPtr<Gdk::Pixmap> pixmap;
    Display *xdisp = GDK_DISPLAY_XDISPLAY(_display->gobj());
    Window xwin    = DefaultRootWindow(xdisp);
    Pixmap* xoldpm = get_current_pixmap(_display);

    // if this is the first time we've set, don't delete/reuse the first pixmap - we hold onto
    // it for possible restore on program exit
    if (xoldpm && !has_set_once) {
        has_set_once = true;
        first_pixmaps[disp] = xoldpm;
        xoldpm = NULL;
    }

    if (xoldpm)
    {
        // grab the old pixmap and ref it into a gdk pixmap
        pixmap = Gdk::Pixmap::create(_display, *xoldpm);

        // check that this pixmap is the right size
        int width, height;
        pixmap->get_size(width, height);

        if ((width != winw) || (height != winh) || (pixmap->get_depth() != wind)) {
            XKillClient(xdisp, *(xoldpm));
            xoldpm = NULL;
        }
    }

    if (!xoldpm) {
        // we have to create it
        pixmap = Gdk::Pixmap::create(window, winw, winh, wind);

        // copy from the first pixmap, if it exists
        if (first_pixmaps.find(disp) != first_pixmaps.end()) {
            Pixmap newxpm = GDK_PIXMAP_XID(pixmap->gobj());
            Glib::RefPtr<Gdk::GC> gc_ = Gdk::GC::create(pixmap);
            GC xgc = GDK_GC_XGC(gc_->gobj());

            xoldpm = first_pixmaps[disp];
            XCopyArea(xdisp, *xoldpm, newxpm, xgc, 0, 0, winw, winh, 0, 0);

            gc_.clear();
        }

        // only set the mode if we never had an old pixmap to work with
        XSetCloseDownMode(xdisp, RetainPermanent);
    }

    pixmap->set_colormap(colormap);
    return pixmap;
}

/**
 * Returns the current X Pixmap set as the background.
 */
Pixmap* SetBG::get_current_pixmap(Glib::RefPtr<Gdk::Display> _display)
{
    Display *xdisp = GDK_DISPLAY_XDISPLAY(_display->gobj());
    Window xwin    = DefaultRootWindow(xdisp);
    Pixmap* xoldpm = NULL;
    Atom prop_root, prop_esetroot, type;
    int format;
    unsigned long length, after;
    unsigned char *data_root, *data_esetroot;

    prop_root = XInternAtom(xdisp, "_XROOTPMAP_ID", True);
    prop_esetroot = XInternAtom(xdisp, "ESETROOT_PMAP_ID", True);

    if (prop_root != None && prop_esetroot != None) {
        XGetWindowProperty(xdisp, xwin, prop_root, 0L, 1L, False, AnyPropertyType, &type, &format, &length, &after, &data_root);
        if (type == XA_PIXMAP) {
            XGetWindowProperty(xdisp, xwin, prop_esetroot, 0L, 1L, False, AnyPropertyType, &type, &format, &length, &after, &data_esetroot);
            if (data_root && data_esetroot)
                if (type == XA_PIXMAP && *((Pixmap *) data_root) == *((Pixmap *) data_esetroot)) {
                    xoldpm = (Pixmap*)data_root;
                }
        }
    }

    return xoldpm;
}

/**
 * Sets the given Pixmap to be the display's permanent background.
 */
void SetBG::set_current_pixmap(Glib::RefPtr<Gdk::Display> _display, Pixmap* new_pixmap)
{
    Display *xdisp = GDK_DISPLAY_XDISPLAY(_display->gobj());
    Window xwin = DefaultRootWindow(xdisp);
    Atom prop_root, prop_esetroot, type;

    prop_root = XInternAtom(xdisp, "_XROOTPMAP_ID", False);
    prop_esetroot = XInternAtom(xdisp, "ESETROOT_PMAP_ID", False);

    if (prop_root == None || prop_esetroot == None) {
        std::cerr << _("ERROR: BG set could not make atoms.") << "\n";
        return;
    }

    XChangeProperty(xdisp, xwin, prop_root, XA_PIXMAP, 32, PropModeReplace, (unsigned char *) &(*new_pixmap), 1);
    XChangeProperty(xdisp, xwin, prop_esetroot, XA_PIXMAP, 32, PropModeReplace, (unsigned char *) &(*new_pixmap), 1);

    // actually set the background via Xlib
    XSetWindowBackgroundPixmap(xdisp, xwin, *new_pixmap);
    XClearWindow(xdisp, xwin);
}

/**
 * Frees the initial pixmaps that were saved on first set.
 *
 * This should be called after save.
 */
void SetBG::clear_first_pixmaps()
{
    for (std::map<Glib::ustring, Pixmap*>::iterator i = first_pixmaps.begin(); i != first_pixmaps.end(); i++) {
        if ((*i).second == NULL)
            continue;

        Glib::RefPtr<Gdk::Display> _display = get_display((*i).first);
        Display *xdisp = GDK_DISPLAY_XDISPLAY(_display->gobj());

        XKillClient(xdisp, *((*i).second));
    }

    first_pixmaps.erase(first_pixmaps.begin(), first_pixmaps.end());
}

/**
 * For each display that has a recorded first pixmap, kills whatever is the current background
 * and resets the background to be the one saved at the intial set time.
 */
void SetBG::reset_first_pixmaps()
{
    for (std::map<Glib::ustring, Pixmap*>::iterator i = first_pixmaps.begin(); i != first_pixmaps.end(); i++) {
        if ((*i).second == NULL)
            continue;

        Glib::RefPtr<Gdk::Display> _display = get_display((*i).first);
        Display *xdisp = GDK_DISPLAY_XDISPLAY(_display->gobj());
        Pixmap *current_pixmap = NULL;

        // get current pixmap from root window
        current_pixmap = get_current_pixmap(_display);

        // if they are the same (how?), no need to do anything
        if ((*i).second == current_pixmap)
           return;

        // ok, they aren't the same. kill the current one
        XKillClient(xdisp, *((Pixmap *) current_pixmap));

        // set the first one back to the prop
        set_current_pixmap(_display, (*i).second);
    }
}

/**
 * Sets a flag to prevent saving the first pixmap.
 * Used by non-interactive modes.
 */
void SetBG::disable_pixmap_save()
{
    has_set_once = true;
}

/**
 * Returns if this background setter should be setting the Nitrogen configuration.
 *
 * Override this in alternate setters that may directly touch external configurations.
 */
bool SetBG::save_to_config()
{
    return true;
}

/*
 * **************************************************************************
 * SetBGXwindows
 * **************************************************************************
 */

/**
 * Gets all active screens on this display.
 * This is used by the main window to determine what to show in the dropdown,
 * if anything.
 *
 * Returns a map of display string to human-readable representation.
 */
std::map<Glib::ustring, Glib::ustring> SetBGXWindows::get_active_displays()
{
    Glib::RefPtr<Gdk::Display> disp = Gdk::DisplayManager::get()->get_default_display();
    std::map<Glib::ustring, Glib::ustring> map_displays;

    for (int i=0; i < disp->get_n_screens(); i++) {
        Glib::RefPtr<Gdk::Screen> screen = disp->get_screen(i);
        std::ostringstream ostr;
        ostr << _("Screen") << " " << i;

        map_displays[screen->make_display_name()] = ostr.str();
    }

    return map_displays;
}

/**
 * Gets the gdk display used by the XWindows specific setter.
 *
 * Has the side effect of setting the disp param.
 */
Glib::RefPtr<Gdk::Display> SetBGXWindows::get_display(Glib::ustring& disp)
{
    Glib::RefPtr<Gdk::Display> _display;

    if (disp != "")
    {
        _display = Gdk::Display::open(disp);
        if (!_display) {
            std::cerr << _("Could not open display") << " " << disp << "\n";
        }
        // @TODO: exception?
    }
    else
    {
        _display = SetBG::get_display(disp);
        disp = _display->get_name();
    }

    return _display;
}

/**
 * Gets the prefix used to save/restore backgrounds with this setter.
 *
 * For XWindows, will be a string of the display eg ":0" or ":1".
 */
Glib::ustring SetBGXWindows::get_prefix()
{
    Glib::ustring display = Gdk::DisplayManager::get()->get_default_display()->get_name();
    return display;
}

/**
 * Gets the full key for "full screen" for this setter.
 *
 * For XWindows, will be a string of the display eg ":0" or ":1".
 * It is exactly the same as the prefix.
 */
Glib::ustring SetBGXWindows::get_fullscreen_key()
{
    return this->get_prefix();
}

/**
 * Make a usable display key to pass to set_bg with a given head number.
 * If no head number given, returns the fullscreen key.
 *
 * For XWindows, will be a string like ":0" or ":0.1"
 */
Glib::ustring SetBGXWindows::make_display_key(gint head)
{
    if (head == -1)
        return this->get_fullscreen_key();

    return Glib::ustring::compose("%1.%2", this->get_prefix(), head);
}

/*
 * **************************************************************************
 * SetBGXinerama
 * **************************************************************************
 */
#ifdef USE_XINERAMA

void SetBGXinerama::set_xinerama_info(XineramaScreenInfo* xinerama_info, gint xinerama_num_screens)
{
    this->xinerama_info = xinerama_info;
    this->xinerama_num_screens = xinerama_num_screens;
}

/**
 * Gets all active screens on this display.
 * This is used by the main window to determine what to show in the dropdown,
 * if anything.
 *
 * Returns a map of display string to human-readable representation.
 */
std::map<Glib::ustring, Glib::ustring> SetBGXinerama::get_active_displays()
{
    std::map<Glib::ustring, Glib::ustring> map_displays;

    // always add full screen as we need the -1 key always
    map_displays[this->get_fullscreen_key()] = _("Full Screen");

    // add individual screens
    for (int i=0; i < xinerama_num_screens; i++) {
        std::ostringstream ostr;
        ostr << _("Screen") << " " << xinerama_info[i].screen_number+1;

        map_displays[this->make_display_key(xinerama_info[i].screen_number)] = ostr.str();
    }

    return map_displays;
}

Glib::ustring SetBGXinerama::get_prefix()
{
    Glib::ustring display("xin_");
    return display;
}

/**
 * Gets the full key for "full screen" for this setter.
 *
 * For Xinerama, this is "xin_-1".
 */
Glib::ustring SetBGXinerama::get_fullscreen_key()
{
    return Glib::ustring::compose("%1-1", this->get_prefix());
}

/**
 * Make a usable display key to pass to set_bg with a given head number.
 * If no head number given, returns the fullscreen key.
 *
 * For Xinerama, will be a string like "xin_-1" (fullscreen) or "xin_0"
 */
Glib::ustring SetBGXinerama::make_display_key(gint head)
{
    return Glib::ustring::compose("%1%2", this->get_prefix(), head);
}

/**
 * Sets the target dimensions based on window dimensions and display.
 *
 * For this Xinerama version, it examines the disp string and uses the stored
 * xinerama info about each screen to set the target.
 */
bool SetBGXinerama::get_target_dimensions(Glib::ustring& disp, gint winx, gint winy, gint winw, gint winh, gint& tarx, gint& tary, gint& tarw, gint& tarh)
{
    gint xin_screen_num;
    int xin_offset = -1;

    // get specific xinerama "screen"
    // disp is a string that should be "xin_#"
    // "xin_-1" refers to the whole thing
    Glib::ustring xin_numstr = disp.substr(4);
    std::stringstream sstr;
    sstr << xin_numstr;
    sstr >> xin_screen_num;

    if (xin_screen_num != -1) {
        for (int i=0; i<xinerama_num_screens; i++)
            if (xinerama_info[i].screen_number == xin_screen_num)
                xin_offset = i;

        if (xin_offset == -1) {
            std::cerr << _("Could not find Xinerama screen number") << " " << xin_screen_num << "\n";
            return false;
        }
    }

	if (xin_screen_num == -1) {
		tarx = winx;
		tary = winy;
		tarw = winw;
		tarh = winh;
	} else {
		tarx = xinerama_info[xin_offset].x_org;
		tary = xinerama_info[xin_offset].y_org;
		tarw = xinerama_info[xin_offset].width;
		tarh = xinerama_info[xin_offset].height;
	}

    return true;
}

#endif

/*
 * **************************************************************************
 * SetBGGnome
 * **************************************************************************
 */

/**
 * Sets the bg if nautilus is appearing to draw the desktop image.
 *
 * Simply calls gconftool-2 for now, until we find a better way to do it.
 */
bool SetBGGnome::set_bg(Glib::ustring &disp, Glib::ustring file, SetMode mode, Gdk::Color bgcolor)
{
    Glib::ustring strmode;
    switch(mode) {
        case SetBG::SET_SCALE:      strmode = "stretched";  break;
        case SetBG::SET_TILE:       strmode = "wallpaper"; break;
        case SetBG::SET_CENTER:     strmode = "centered"; break;
        case SetBG::SET_ZOOM:       strmode = "scaled"; break;
        case SetBG::SET_ZOOM_FILL:  strmode = "spanned"; break;
        default:                    strmode = "zoom"; break;
	};

    Glib::RefPtr<Gio::Settings> settings = Gio::Settings::create(get_gsettings_key());

    Glib::RefPtr<Gio::File> iofile = Gio::File::create_for_commandline_arg(file);

    settings->set_string("picture-uri", iofile->get_uri());
    settings->set_string("picture-options", strmode);
    settings->set_string("primary-color", bgcolor.to_string());
    settings->set_string("secondary-color", bgcolor.to_string());

    set_show_desktop();

    return true;
}

std::map<Glib::ustring, Glib::ustring> SetBGGnome::get_active_displays()
{
    std::map<Glib::ustring, Glib::ustring> map_displays;
    map_displays["dummy"] = "Gnome";
    return map_displays;
}

Glib::ustring SetBGGnome::get_prefix()
{
    Glib::ustring display("");
    return display;
}

/**
 * Gets the full key for "full screen" for this setter.
 * This is N/A i think?
 */
Glib::ustring SetBGGnome::get_fullscreen_key()
{
    Glib::ustring display("dummy");
    return display;
}

/**
 * Make a usable display key to pass to set_bg with a given head number.
 * If no head number given, returns the fullscreen key.
 */
Glib::ustring SetBGGnome::make_display_key(gint head)
{
    return Glib::ustring("dummy");
}

/**
 * Returns the schema key to be used for setting background settings.
 *
 * Can be overridden.
 */
Glib::ustring SetBGGnome::get_gsettings_key()
{
    return Glib::ustring("org.gnome.desktop.background");
}

/**
 * Sets the show_desktop flag in the appropriate spot.
 *
 * This varies depending on actual software used, so needs to be own function
 * to be overridden.
 */
void SetBGGnome::set_show_desktop()
{
    Glib::RefPtr<Gio::Settings> settings = Gio::Settings::create(get_gsettings_key());
    std::vector<Glib::ustring> keys = settings->list_keys();

    if (std::find(keys.begin(), keys.end(), Glib::ustring("draw-background")) != keys.end()) {
        try {
            settings->set_boolean("draw-background", true);
        } catch (...) {
            std::cerr << "ERROR: draw-background supposedly exists, but threw an error while setting it\n";
        }
    }
}

/**
 * Returns if this background setter should be setting the Nitrogen configuration.
 *
 * The Gnome mode is completely external.
 */
bool SetBGGnome::save_to_config()
{
    return false;
}

/*
 * **************************************************************************
 * SetBGNemo
 * **************************************************************************
 */
/**
 * Returns the schema key to be used for setting background settings.
 *
 * Can be overridden.
 */
Glib::ustring SetBGNemo::get_gsettings_key()
{
    return Glib::ustring("org.cinnamon.desktop.background");
}

/**
 * Sets the show_desktop flag in the appropriate spot.
 *
 * This varies depending on actual software used, so needs to be own function
 * to be overridden.
 */
void SetBGNemo::set_show_desktop()
{
    // this space left blank as show-desktop no longer a thing in Nemo
}

/**
 * Returns if this background setter should be setting the Nitrogen configuration.
 *
 * The Nemo mode is completely external.
 */
bool SetBGNemo::save_to_config()
{
    return false;
}

/*
 * **************************************************************************
 * SetBGPcmanfm
 * **************************************************************************
 */

bool SetBGPcmanfm::set_bg(Glib::ustring &disp, Glib::ustring file, SetMode mode, Gdk::Color bgcolor)
{
	Atom ret_type;
    int ret_format;
    gulong ret_items;
    gulong ret_bytesleft;
	guchar *data;
    Glib::ustring strmode;
    switch(mode) {
        case SetBG::SET_SCALE:      strmode = "stretch";  break;
        case SetBG::SET_TILE:       strmode = "tile"; break;
        case SetBG::SET_CENTER:     strmode = "center"; break;
        case SetBG::SET_ZOOM:       strmode = "fit"; break;
        case SetBG::SET_ZOOM_FILL:  strmode = "crop"; break;
        default:                    strmode = "fit"; break;
	};

    // default to "LXDE" for profile name
    Glib::ustring profileName = Glib::ustring("LXDE");

    // find pcmanfm process (pull off root window)
    Glib::RefPtr<Gdk::Display> display = Gdk::DisplayManager::get()->get_default_display();
    Display* xdisp = GDK_DISPLAY_XDISPLAY(display->gobj());

    SetBG::RootWindowData rootData = get_root_window_data(display);

    if (rootData.type == SetBG::PCMANFM) {
        // pull PID atom from pcman desktop window
        Window curwindow = rootData.window;

        Atom propatom = XInternAtom(xdisp, "_NET_WM_PID", False);

        int result = XGetWindowProperty(xdisp,
                                        curwindow,
                                        propatom,
                                        0, G_MAXLONG,
                                        False, XA_CARDINAL, &ret_type, &ret_format, &ret_items, &ret_bytesleft, &data);

        if (result != Success) {
            std::cerr << "ERROR: Could not determine pid of Pcmanfm desktop window, is _NET_WM_PID set on X root window?\n";
            return false;
        }

        long pid = 0;

        if (ret_type == XA_CARDINAL && ret_format == 32 && ret_items == 1) {
            pid = ((long* )data)[0];
            XFree(data);
        }

        if (pid > 0) {

            // attempt to pull profile name from pcmanfm process command line
            std::ostringstream ss;
            ss << pid;

            std::string filename = Glib::build_filename("/", "proc", ss.str(), "cmdline");
            if (Glib::file_test(filename, Glib::FILE_TEST_EXISTS)) {
                std::string contents = Glib::file_get_contents(filename);

                // contents is a string with null characters. glib doesn't seem to like splitting on the null character,
                // so we have to do it manually.
                std::vector<Glib::ustring> splits;

                std::string::const_iterator i = contents.begin();
                while (i != contents.end()) {
                    std::string::const_iterator next = std::find(i, (std::string::const_iterator)contents.end(), '\0');
                    splits.push_back(std::string(i, next));
                    i = next + 1;
                }

                // use our own ArgParse
                ArgParser* parser = new ArgParser();
                parser->register_option("profile", "", true);
                parser->register_option("desktop");

                if (parser->parse(splits)) {
                   if (parser->has_argument("profile"))
                       profileName = parser->get_value("profile");
                   else
                       // found pcmanfm exe, but no profile arg?  use 'default' instead of LXDE
                       profileName = std::string("default");
                }

                // cleanup
                delete parser;
            }

            // find configuration file in profile directory (do a minimal create if needed)
            // mimics logic in pcmanfm_get_profile_dir
            std::string configdir = Glib::build_filename(Glib::get_user_config_dir(), "pcmanfm", profileName);
            if (!Glib::file_test(configdir, Glib::FILE_TEST_EXISTS)) {
                Glib::RefPtr<Gio::File> giof = Gio::File::create_for_path(configdir);
                giof->make_directory_with_parents();
            }

            // SPECIAL HANDLING: for "full screen", all configs must be set to the same bg and stretch across mode
            if (disp == this->get_fullscreen_key()) {

                // iterate all screens
                std::map<Glib::ustring, Glib::ustring> map_displays = this->get_active_displays();
                for (std::map<Glib::ustring, Glib::ustring>::const_iterator i = map_displays.begin(); i != map_displays.end(); i++) {

                    // skip fullscreen key, there's no config file for that
                    if (i->first == disp)
                        continue;

                    // read config file, set, save
                    std::string configfile = Glib::build_filename(Glib::get_user_config_dir(), "pcmanfm", profileName, Glib::ustring::compose("desktop-items-%1.conf", i->first));

                    Glib::KeyFile kf;
                    kf.load_from_file(configfile);

                    kf.set_string("*", "wallpaper_mode", "screen");
                    kf.set_string("*", "wallpaper_common", "1");
                    kf.set_string("*", "wallpaper", file);
                    kf.set_string("*", "desktop_bg", Util::color_to_string(bgcolor));

                    if (kf.has_key("*", "wallpapers_configured"))
                        kf.remove_key("*", "wallpapers_configured");
                    if (kf.has_key("*", "wallpaper0"))
                        kf.remove_key("*", "wallpaper0");

                    kf.save_to_file(configfile);
                }

            } else {
                // read config file, set, save
                std::string configfile = Glib::build_filename(Glib::get_user_config_dir(), "pcmanfm", profileName, Glib::ustring::compose("desktop-items-%1.conf", disp));

                Glib::KeyFile kf;
                kf.load_from_file(configfile);

                kf.set_string("*", "wallpaper_mode", strmode);
                kf.set_string("*", "wallpaper_common", "0");
                kf.set_string("*", "wallpapers_configured", "1");
                kf.set_string("*", "wallpaper0", file);
                kf.set_string("*", "desktop_bg", Util::color_to_string(bgcolor));

                if (kf.has_key("*", "wallpaper"))
                    kf.remove_key("*", "wallpaper");

                kf.save_to_file(configfile);
            }

            // send USR1 to pcmanfm
            kill(pid, SIGUSR1);
        } else {
            std::cerr << "ERROR: _NET_WM_PID set on X root window, but pid not readable.\n";
            return false;
        }
    } else {
        // @TODO: what happened?
    }

    return true;
}

/**
 * Gets all active screens on this display.
 * This is used by the main window to determine what to show in the dropdown,
 * if anything.
 *
 * Returns a map of display string to human-readable representation.
 */
std::map<Glib::ustring, Glib::ustring> SetBGPcmanfm::get_active_displays()
{
    Glib::RefPtr<Gdk::Display> disp = Gdk::DisplayManager::get()->get_default_display();
    std::map<Glib::ustring, Glib::ustring> map_displays;

    map_displays[this->get_fullscreen_key()] = _("Full Screen");

    for (int i=0; i < disp->get_n_screens(); i++) {
        Glib::RefPtr<Gdk::Screen> screen = disp->get_screen(i);

        for (int j=0; j < screen->get_n_monitors(); j++) {
            std::ostringstream ostr;
            ostr << _("Screen") << " " << j;

            map_displays[this->make_display_key(j)] = ostr.str();
        }
    }

    return map_displays;
}

/**
 * Gets the full key for "full screen" for this setter.
 *
 * For Pcmanfm, simply "-1".
 */
Glib::ustring SetBGPcmanfm::get_fullscreen_key() {
    return this->make_display_key(-1);
}

/*
 * Make a usable display key to pass to set_bg with a given head number.
 *
 * For Pcmanfm, return simply the head number.
 */
Glib::ustring SetBGPcmanfm::make_display_key(gint head) {
    return Glib::ustring::compose("%1", head);
}

/**
 * Returns if this background setter should be setting the Nitrogen configuration.
 *
 * The Pcmanfm mode is completely external.
 */
bool SetBGPcmanfm::save_to_config()
{
    return false;
}

/*
 * **************************************************************************
 * SetBGXFCE
 * **************************************************************************
 */

bool SetBGXFCE::set_bg(Glib::ustring &disp, Glib::ustring file, SetMode mode, Gdk::Color bgcolor) {
    Glib::ustring strmode = "1";
	switch(mode) {
		case SetBG::SET_CENTER:     strmode = "1"; break;
		case SetBG::SET_TILE:       strmode = "2"; break;
		case SetBG::SET_SCALE:      strmode = "3"; break;   // xfce calls this "stretched"
		case SetBG::SET_ZOOM:       strmode = "4"; break;   // xfce calls this "scaled"
		case SetBG::SET_ZOOM_FILL:  strmode = "5"; break;   // xfce calls this "zoomed"
	};

    if (disp == this->get_fullscreen_key()) {
        // special handling for fullscreen:
        // XFCE sets a new mode (value "6", labeled "Spanning Screens" in their config tool), and
        // sets it on monitor0.  just tweak the params here and continue
        disp    = std::string("0");
        strmode = std::string("6");
    } else {
        // special checking: make sure it's in active displays, or we'll accidentally set brand new keys
        std::map<Glib::ustring, Glib::ustring> active_displays = this->get_active_displays();
        if (active_displays.find(disp) == active_displays.end()) {
            std::cerr << Glib::ustring::compose("Unknown display value (%1) for XFCE", disp) << "\n";
            return false;
        }
    }

    // set image
    std::vector<std::string> params;
    params.push_back(std::string("-s"));
    params.push_back(std::string(file));

    call_xfconf(disp, std::string("last-image"), params);

    params.clear();
    params.push_back(std::string("-s"));
    params.push_back(strmode);
    call_xfconf(disp, std::string("image-style"), params);

    // set color mode
    // we only support "solid" color mode (for now!)
    params.clear();
    params.push_back(std::string("-s"));
    params.push_back(std::string("0"));
    call_xfconf(disp, std::string("color-style"), params);

    // set color
    params.clear();
    params.push_back(std::string("--create"));
    params.push_back(std::string("--type"));
    params.push_back(std::string("uint"));
    params.push_back(std::string("--type"));
    params.push_back(std::string("uint"));
    params.push_back(std::string("--type"));
    params.push_back(std::string("uint"));
    params.push_back(std::string("--type"));
    params.push_back(std::string("uint"));
    params.push_back(std::string("-s"));
    params.push_back(Glib::ustring::compose("%1", bgcolor.get_red()));     // r
    params.push_back(std::string("-s"));
    params.push_back(Glib::ustring::compose("%1", bgcolor.get_green()));     // g
    params.push_back(std::string("-s"));
    params.push_back(Glib::ustring::compose("%1", bgcolor.get_blue()));     // b
    params.push_back(std::string("-s"));
    params.push_back(std::string("65535")); // a
    call_xfconf(disp, std::string("color1"), params);

	return true;
}

/*
 * Make a usable display key to pass to set_bg with a given head number.
 *
 * For XFCE, return simply the head/monitor number.
 */
Glib::ustring SetBGXFCE::make_display_key(gint head)
{
    if (head == -1)
        return this->get_fullscreen_key();

    return Glib::ustring::compose("%1", head);
}

/**
 * Gets all active screens on this display.
 * This is used by the main window to determine what to show in the dropdown,
 * if anything.
 *
 * Returns a map of display string to human-readable representation.
 */
std::map<Glib::ustring, Glib::ustring> SetBGXFCE::get_active_displays()
{
    std::map<Glib::ustring, Glib::ustring> map_displays;

    // execute xfconf-query, listing keys in -c xfce4-desktop, extract screenX/monitorY entries (just use the monitors)
    std::vector<std::string> vecCmdLine;
    vecCmdLine.push_back(std::string("xfconf-query"));
    vecCmdLine.push_back(std::string("-c"));
    vecCmdLine.push_back(std::string("xfce4-desktop"));
    vecCmdLine.push_back(std::string("-l"));

    std::string so;
    try {
        Glib::spawn_sync("", vecCmdLine, Glib::SPAWN_SEARCH_PATH, sigc::slot<void>(), &so, NULL, NULL);
    }
    catch (Glib::SpawnError e) {
		std::cerr << _("ERROR") << "\n" << e.what() << "\n";

        for (std::vector<std::string>::const_iterator i = vecCmdLine.begin(); i != vecCmdLine.end(); i++)
			std::cerr << *i << " ";

		std::cerr << "\n";
        return map_displays;
    }

    std::vector<std::string> lines = Glib::Regex::split_simple("\n", so);
    Glib::RefPtr<Glib::Regex> rMonitor = Glib::Regex::create("monitor(\\d+)");
    Glib::MatchInfo info;

    // augment this info with info we get from Xinerama @TODO use xrandr
    // trying to catch cases where:
    // - XFCE config has not yet been generated for a head (add to map)
    // - a head unplugged (remove from map!)
    std::vector<std::string> xin_heads;

#ifdef USE_XINERAMA
    XineramaScreenInfo *xinerama_info;
    gint xinerama_num_screens;
    Glib::RefPtr<Gdk::Display> dpy = Gdk::DisplayManager::get()->get_default_display();

    xinerama_info = XineramaQueryScreens(GDK_DISPLAY_XDISPLAY(dpy->gobj()), &xinerama_num_screens);

    // populate xin_heads, so we don't have to use ifdef guards further down
    for (int i=0; i < xinerama_num_screens; i++) {
      std::ostringstream ostr;
      ostr << xinerama_info[i].screen_number;
      xin_heads.push_back(ostr.str());
    }
#endif

    for (std::vector<std::string>::const_iterator i = lines.begin(); i != lines.end(); i++) {

        bool ok = rMonitor->match(*i, info);
        if (ok) {
            std::string monitorNum = info.fetch(1);

            // add it if:
            // - it's not already in the seen display list
            // - we have items in xin_heads and this display is in that set
            if (map_displays.find(monitorNum) == map_displays.end() && (xin_heads.empty() ? true : std::find(xin_heads.begin(), xin_heads.end(), monitorNum) != xin_heads.end())) {
                guint monitorNumInt;
                std::istringstream inpstream(monitorNum);
                inpstream >> monitorNumInt;

                std::ostringstream ostr;
                ostr << _("Screen") << " " << (monitorNumInt+1);

                map_displays[monitorNum] = ostr.str();
            }
        }
    }

    if (map_displays.size() > 1) {
        map_displays[this->get_fullscreen_key()] = _("Full Screen");
    }

    return map_displays;
}

/**
 * Gets the full key for "full screen" for this setter.
 *
 * For XFCE, simply "fullscreen".
 */
Glib::ustring SetBGXFCE::get_fullscreen_key()
{
    return Glib::ustring("fullscreen");
}

Glib::ustring SetBGXFCE::get_prefix()
{
    Glib::ustring display("");
    return display;
}

/**
 * Returns if this background setter should be setting the Nitrogen configuration.
 *
 * The XFCE mode is completely external.
 */
bool SetBGXFCE::save_to_config()
{
    return false;
}

/**
 * Helper method to call xfconf asynchronously.
 *
 * Settings in XFCE appear to only be settable one at a time, so we have to call this method
 * repeatedly to set all the various parameters Nitrogen handles.
 *
 * Expects to set properties in the form "/backdrop/screen0/monitor<X>/workspace0/<key>", where
 * the monitor number is set by the `disp` param.
 *
 * @param   disp    The display, which translates into monitorX.
 * @param   key     The name of the property key to set.
 * @param   params  Parameters (typically ["-s", "some value"]).
 * @return          Success or if it had an issue with spawn.
 */
bool SetBGXFCE::call_xfconf(Glib::ustring disp, std::string key, const std::vector<std::string>& params) {
    std::vector<std::string> vecCmdLine;
    vecCmdLine.push_back(std::string("xfconf-query"));
    vecCmdLine.push_back(std::string("-c"));
    vecCmdLine.push_back(std::string("xfce4-desktop"));
    vecCmdLine.push_back(std::string("-p"));
    vecCmdLine.push_back(Glib::ustring::compose("/backdrop/screen0/monitor%1/workspace0/%2", disp, key));
    vecCmdLine.insert(vecCmdLine.end(), params.begin(), params.end());

    try {
        Glib::spawn_async(std::string(""), vecCmdLine, Glib::SPAWN_SEARCH_PATH);
    }
    catch (Glib::SpawnError e) {
		std::cerr << _("ERROR") << "\n" << e.what() << "\n";

        for (std::vector<std::string>::const_iterator i = vecCmdLine.begin(); i != vecCmdLine.end(); i++)
			std::cerr << *i << " ";

		std::cerr << "\n";

        return false;
	}
}
