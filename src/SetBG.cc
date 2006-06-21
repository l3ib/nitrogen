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

// for the linker
//Glib::RefPtr<Gdk::Display> SetBG::_display;
//bool SetBG::_opened = false;

/**
 * Sets the background to the image in the file specified.
 *
 * @param	disp	The display to set it on
 * @param	file	The file to set the bg to
 * @param	mode	Stretch, tile, center, bestfit
 * @param	bgcolor	The background color for modes that do not cover entire portions
 * @return			If it went smoothly
 */
bool SetBG::set_bg(	Glib::ustring disp,	Glib::ustring file, SetMode mode, Gdk::Color bgcolor) {

	gint winx,winy,winw,winh,wind;
	Glib::RefPtr<Gdk::GC> gc_;
	Glib::RefPtr<Gdk::Window> window;
	Glib::RefPtr<Gdk::Colormap> colormap;
	Glib::RefPtr<Gdk::Pixbuf> pixbuf, outpixbuf;
	Glib::RefPtr<Gdk::Pixmap> pixmap;
	Glib::RefPtr<Gdk::Screen> screen;

	// see if this display currently exists
	//SetBG::_display = Gdk::Display::open(disp);
	
	// open display and screen
	Glib::RefPtr<Gdk::Display> _display = Gdk::Display::open(disp);
	if (!_display) {
		std::cerr << "Could not open display " << disp << "\n";
		return false;
	}

	screen = _display->get_default_screen();

	/*
	Glib::RefPtr<Gdk::DisplayManager> manager = Gdk::DisplayManager::get();
	std::vector< Glib::RefPtr<Gdk::Display> > displays = std::vector< Glib::RefPtr<Gdk::Display> >(manager->list_displays());
	std::vector< Glib::RefPtr<Gdk::Display> >::const_iterator iter;
	
	for (iter=displays.begin();iter!=displays.end();iter++) {
		if ((*iter)->get_name() == disp) 
			break;
	}

	if ( iter == displays.end() ) {
		std::cerr << "Display " << disp << " is not openable." << std::endl;
		return false;		
	}
	*/
	
	
	
/*	
	if ( ! SetBG::_opened ) {
		SetBG::_display = Gdk::Display::open(disp);
		if (!SetBG::_display) {
			return false;
		}
		SetBG::_opened = true;
		screen = SetBG::_display->get_default_screen();
	} else {
		screen = SetBG::_display->get_default_screen();
		// make sure we are on right display
		if ( screen->make_display_name() != disp ) {
			std::cout << "not right disp\n";
			SetBG::_display = Gdk::Display::open(disp);
			std::cout << "opened disp\n";
			if (SetBG::_display == 0) {
				return false;
			}
			std::cout << "getting screen\n";
			screen = SetBG::_display->get_default_screen();
			std::cout << "got screen\n";
		}
	}
*/
	// get window stuff
	window = screen->get_root_window();
	window->get_geometry(winx,winy,winw,winh,wind);

	// create gc and colormap
	gc_ = Gdk::GC::create(window);
	colormap = screen->get_default_colormap();

	// allocate bg color
	colormap->alloc_color(bgcolor, false, true);

	// create pixmap 
	pixmap = Gdk::Pixmap::create(window,winw,winh,window->get_depth());
	pixmap->set_colormap(colormap);
	
	// get our pixbuf from the file
	try {
		pixbuf = Gdk::Pixbuf::create_from_file(file);	
	} catch (Glib::FileError e) {
		std::cerr << "ERROR: Could not load file in bg set: " << e.what() << "\n";
		return false;
	}

	// manipulate it with what we need
	
	switch(mode) {
	
		case SetBG::SET_SCALE:
			outpixbuf = SetBG::make_scale(pixbuf, winw, winh, bgcolor);
			break;

		case SetBG::SET_TILE:
			outpixbuf = SetBG::make_tile(pixbuf, winw, winh, bgcolor);
			break;

		case SetBG::SET_CENTER:
			outpixbuf = SetBG::make_center(pixbuf, winw, winh, bgcolor);
			break;

		case SetBG::SET_BEST:
			outpixbuf = SetBG::make_best(pixbuf, winw, winh, bgcolor);
			break;
			
		default:
			std::cerr << "Unknown mode for saved bg on " << disp << std::endl;
			return false;
	};

	// render it to the pixmap
	pixmap->draw_pixbuf(gc_, outpixbuf, 0,0,0,0, winw, winh, Gdk::RGB_DITHER_NONE,0,0);
	
	// begin the X fun!
	// /*GDK_DISPLAY_XDISPLAY(_display->gobj());*/ /*XOpenDisplay(disp.c_str());*/
	Display *xdisp = GDK_DISPLAY_XDISPLAY(_display->gobj());
	XSetCloseDownMode(xdisp, RetainPermanent);
	Window xwin = DefaultRootWindow(xdisp);
	Pixmap xpm = GDK_PIXMAP_XID(pixmap->gobj());
	
	// FROM HERE, mostly ported from feh and 
	// http://www.eterm.org/docs/view.php?doc=ref#trans
	
	Atom prop_root, prop_esetroot;
			/*, type;
	int format;
	unsigned long length, after;
	unsigned char *data_root, *data_esetroot;
	*/
	// set and make persistant
	// TODO: 100% BUG FILLED SHIT.  PLZ FIGURE OUT WHY XFREEPIXMAP DOES NOT WORK.
	// actually,there may not be a leak at all with this.  
/*
	prop_root = XInternAtom(xdisp, "_XROOTPMAP_ID", True);
	prop_esetroot = XInternAtom(xdisp, "ESETROOT_PMAP_ID", True);
	
	if (prop_root != None && prop_esetroot != None) {
		XGetWindowProperty(xdisp, xwin, prop_root, 0L, 1L, False, AnyPropertyType, &type, &format, &length, &after, &data_root);
		if (type == XA_PIXMAP) {
			XGetWindowProperty(xdisp, xwin, prop_esetroot, 0L, 1L, False, AnyPropertyType, &type, &format, &length, &after, &data_esetroot);
			if (data_root && data_esetroot)
				if (type == XA_PIXMAP && *((Pixmap *) data_root) == *((Pixmap *) data_esetroot)) {
					//XKillClient(xdisp, *((Pixmap *) data_root));
					//std::cout << "Shoulda killed\n";
					XFreePixmap(xdisp, *((Pixmap*) data_root));
					printf("Test says to remove %x\n", *((Pixmap*) data_root));
				}
		}
	}
*/
	prop_root = XInternAtom(xdisp, "_XROOTPMAP_ID", False);
	prop_esetroot = XInternAtom(xdisp, "ESETROOT_PMAP_ID", False);

	if (prop_root == None || prop_esetroot == None) {
		std::cerr << "ERROR: BG set could not make atoms.\n";
		return false;
	}

	XChangeProperty(xdisp, xwin, prop_root, XA_PIXMAP, 32, PropModeReplace, (unsigned char *) &xpm, 1);
	XChangeProperty(xdisp, xwin, prop_esetroot, XA_PIXMAP, 32, PropModeReplace, (unsigned char *) &xpm, 1);

	// set it gtk style
	window->set_back_pixmap(pixmap, false);
	window->clear();

	// close display  //No need the refptr does it apparantly
	_display->flush();
/*	
	std::cerr << "preclose\n";
	_display->close();
	std::cerr << "we made it here\n";
*/	

	//_display = Gdk::Display::open(":0.0"); 

	//_display->close();
	//XCloseDisplay(xdisp);
	
	return true;
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
 * TODO: figure out how to do this
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
 * Handles SET_BEST mode.
 *
 * @param	orig	The original pixbuf
 * @param	winw	Width of the window
 * @param	winh	Height of the window
 * @param	bgcolor	Background color
 */
Glib::RefPtr<Gdk::Pixbuf> SetBG::make_best(const Glib::RefPtr<Gdk::Pixbuf> orig, const gint winw, const gint winh, Gdk::Color bgcolor) {
		
	int x, y, resx, resy;
	x = y = 0;
		
	// depends on bigger side
	if ( orig->get_width() > orig->get_height() ) {
			
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

	// resize to a temp
	Glib::RefPtr<Gdk::Pixbuf> tmp = orig->scale_simple(resx, resy, Gdk::INTERP_BILINEAR);

	Glib::RefPtr<Gdk::Pixbuf> retval = Gdk::Pixbuf::create(	orig->get_colorspace(),
															orig->get_has_alpha(),
															orig->get_bits_per_sample(),
															winw,
															winh);

	// use passed bg color
	retval->fill(GdkColorToUint32(bgcolor));

	// copy it in
	tmp->copy_area(0, 0, tmp->get_width(), tmp->get_height(), retval, x, y);

	return retval;
}		

/**
 * Utility function to convert a mode (an enum) to a string. 
 */
Glib::ustring SetBG::mode_to_string( const SetMode mode ) {

	Glib::ustring ret;
	
	switch ( mode ) {
		case SET_SCALE:
			ret = Glib::ustring("Scale");
			break;
		case SET_CENTER:
			ret = Glib::ustring("Center");
			break;
		case SET_TILE:
			ret = Glib::ustring("Tile");
			break;
		case SET_BEST:
			ret = Glib::ustring("Best");
			break;
	};
	
	return ret;
}

/**
 * Utility function to translate to SetMode from a string.  Meant to be used from the values
 * produced in the above function.
 */
SetBG::SetMode SetBG::string_to_mode( const Glib::ustring str ) {

	if ( str == Glib::ustring("Scale") )
		return SetBG::SET_SCALE;
	else if ( str == Glib::ustring("Center") )
		return SetBG::SET_CENTER;
	else if ( str == Glib::ustring("Tile") )
		return SetBG::SET_TILE;
	else if ( str == Glib::ustring("Best") )
		return SetBG::SET_BEST;

	// shouldn't get here
	return SetBG::SET_SCALE;
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

