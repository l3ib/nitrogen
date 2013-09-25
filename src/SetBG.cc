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

using namespace Util;

/**
 * Factory method to produce a concrete SetBG derived class.
 * You are responsible for freeing the result.
 */
SetBG* SetBG::get_bg_setter()
{
    Glib::RefPtr<Gdk::Display> dpy = Gdk::DisplayManager::get()->get_default_display();

    SetBG* setter = NULL;
    SetBG::RootWindowType wt = SetBG::get_rootwindowtype(dpy);

    switch (wt) {
        case SetBG::NAUTILUS:
            setter = new SetBGGnome();
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
        case SetBG::DEFAULT:
        default:
            setter = new SetBGXWindows();
            break;
    };

    return setter;
}

/**
 * Handles a BadWindow error in get_rootwindowtype.
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
 * Determines if Nautilus is being used to draw the root desktop.
 *
 * @returns 	True if nautilus is drawing the desktop.
 */
SetBG::RootWindowType SetBG::get_rootwindowtype(Glib::RefPtr<Gdk::Display> display)
{
	GdkAtom type;
    gint format;
    gint length;
	guchar *data;
    SetBG::RootWindowType retval = SetBG::DEFAULT;
    gboolean ret = FALSE;
    Glib::RefPtr<Gdk::Window> rootwin;

    rootwin = display->get_default_screen()->get_root_window();

	ret =    gdk_property_get(rootwin->gobj(),
						      gdk_atom_intern("NAUTILUS_DESKTOP_WINDOW_ID", FALSE),
                              gdk_atom_intern("WINDOW", FALSE),
							  0,
							  4, /* length of a window is 32bits*/
							  FALSE, &type, &format, &length, &data);

    if (!ret) {
#ifdef USE_XINERAMA
        // determine if xinerama is in play
        //
        XineramaScreenInfo *xinerama_info;
        gint xinerama_num_screens;

        xinerama_info = XineramaQueryScreens(GDK_DISPLAY_XDISPLAY(display->gobj()), &xinerama_num_screens);

        if (xinerama_num_screens > 1)
            return SetBG::XINERAMA;
#endif

        return SetBG::DEFAULT;
    }

    guint wid = *(guint*)data;

    Display *xdisp = GDK_DISPLAY_XDISPLAY(rootwin->get_display()->gobj());
    Atom propatom = XInternAtom(xdisp, "WM_CLASS", FALSE);

    XTextProperty tprop;

    gchar **list;
    gint num;

    XErrorHandler old_x_error_handler = XSetErrorHandler(SetBG::handle_x_errors);
    bool bGotText = XGetTextProperty(xdisp, wid, &tprop, propatom);
    XSetErrorHandler(old_x_error_handler);

    if (bGotText && tprop.nitems)
    {
        if (XTextPropertyToStringList(&tprop, &list, &num))
        {
            // expect 2 strings here (XLib tells us there are 3)
            if (num != 3)
                retval = SetBG::DEFAULT;
            else
            {
                std::string strclass = std::string(list[1]);
                if (strclass == std::string("Xfdesktop")) retval = SetBG::XFCE;     else
                if (strclass == std::string("Nautilus"))  retval = SetBG::NAUTILUS; else
                {
                    std::cerr << _("UNKNOWN ROOT WINDOW TYPE DETECTED, will attempt to set via normal X procedure") << "\n";
                    retval = SetBG::UNKNOWN;
                }

         }

            XFreeStringList(list);
        }
        XFree(tprop.value);
    }

    g_free(data);
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
Glib::RefPtr<Gdk::Display> SetBG::get_display(Glib::ustring& disp)
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
    this->get_target_dimensions(disp, winx, winy, winw, winh, tarx, tary, tarw, tarh);

    // create gc and colormap
    gc_ = Gdk::GC::create(window);
    colormap = screen->get_default_colormap();

    // allocate bg color
    colormap->alloc_color(bgcolor, false, true);

    // get or create pixmap - for xinerama, may use existing one to merge
    // this will delete the old pixmap if necessary
    // also sets shutdown mode if necessary
    pixmap = this->get_or_create_pixmap(_display, window, winw, winh, wind, colormap);

    // get our pixbuf from the file
    try {
        pixbuf = Gdk::Pixbuf::create_from_file(file);
    } catch (Glib::FileError e) {
        std::cerr << _("ERROR: Could not load file in bg set") << ": " << e.what() << "\n";
        return false;
    }

    outpixbuf = this->make_resized_pixbuf(pixbuf, mode, bgcolor, tarw, tarh);

    // render it to the pixmap
	pixmap->draw_pixbuf(gc_, outpixbuf, 0, 0, tarx, tary, tarw, tarh, Gdk::RGB_DITHER_NONE, 0, 0);

    // set it via XLib
    Pixmap xpm = GDK_PIXMAP_XID(pixmap->gobj());

    prop_root = XInternAtom(xdisp, "_XROOTPMAP_ID", False);
    prop_esetroot = XInternAtom(xdisp, "ESETROOT_PMAP_ID", False);

    if (prop_root == None || prop_esetroot == None) {
        std::cerr << _("ERROR: BG set could not make atoms.") << "\n";
        return false;
    }

    XChangeProperty(xdisp, xwin, prop_root, XA_PIXMAP, 32, PropModeReplace, (unsigned char *) &xpm, 1);
    XChangeProperty(xdisp, xwin, prop_esetroot, XA_PIXMAP, 32, PropModeReplace, (unsigned char *) &xpm, 1);

    // set it via gtk
    window->set_back_pixmap(pixmap, false);
    window->clear();

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
void SetBG::get_target_dimensions(Glib::ustring& disp, gint winx, gint winy, gint winw, gint winh, gint& tarx, gint& tary, gint& tarw, gint& tarh)
{
    tarx = winx;
    tary = winy;
    tarw = winw;
    tarh = winh;
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
Glib::RefPtr<Gdk::Pixmap> SetBG::get_or_create_pixmap(Glib::RefPtr<Gdk::Display> _display, Glib::RefPtr<Gdk::Window> window, gint winw, gint winh, gint wind, Glib::RefPtr<Gdk::Colormap> colormap)
{
    Glib::RefPtr<Gdk::Pixmap> pixmap;
    Display *xdisp = GDK_DISPLAY_XDISPLAY(_display->gobj());
    Window xwin = DefaultRootWindow(xdisp);
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

    if (xoldpm)
    {
        // grab the old pixmap and ref it into a gdk pixmap
        pixmap = Gdk::Pixmap::create(_display, *xoldpm);

        // check that this pixmap is the right size
        int width, height;
        pixmap->get_size(width, height);

        if ((width != winw) || (height != winh) || (pixmap->get_depth() != wind)) {
            XKillClient(xdisp, *((Pixmap *) data_root));
            xoldpm = NULL;
        }
    }

    if (!xoldpm) {
        // we have to create it
        pixmap = Gdk::Pixmap::create(window, winw, winh, wind);

        // only set the mode if we never had an old pixmap to work with
        XSetCloseDownMode(xdisp, RetainPermanent);
    }

    pixmap->set_colormap(colormap);
    return pixmap;
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

    // if at least 2 screens, add the full screen
    // reason being if xinerama on but only one screen, full screen is the same as the first display
    if (this->xinerama_num_screens > 1)
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
void SetBGXinerama::get_target_dimensions(Glib::ustring& disp, gint winx, gint winy, gint winw, gint winh, gint& tarx, gint& tary, gint& tarw, gint& tarh)
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
            return;  // @TODO: throw?
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
        case SetBG::SET_ZOOM_FILL:
        default:                    strmode = "zoom"; break;
	};

    Glib::RefPtr<Gio::Settings> settings = Gio::Settings::create("org.gnome.desktop.background");

    Glib::RefPtr<Gio::File> iofile = Gio::File::create_for_commandline_arg(file);

    settings->set_string("picture-uri", iofile->get_uri());
    settings->set_string("picture-options", strmode);
    settings->set_string("primary-color", bgcolor.to_string());
    settings->set_string("secondary-color", bgcolor.to_string());
    settings->set_boolean("draw-background", true);

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
    Glib::ustring display("????");
    return display;
}

/**
 * Gets the full key for "full screen" for this setter.
 * This is N/A i think?
 */
Glib::ustring SetBGGnome::get_fullscreen_key()
{
    Glib::ustring display("????");
    return display;
}

/**
 * Make a usable display key to pass to set_bg with a given head number.
 * If no head number given, returns the fullscreen key.
 */
Glib::ustring SetBGGnome::make_display_key(gint head)
{
    return Glib::ustring("");
}

