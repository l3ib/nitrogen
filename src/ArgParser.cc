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

#include "ArgParser.h"
#include "gcs-i18n.h"

bool ArgParser::parse
			(int argc, char ** argv) {
    std::vector<Glib::ustring> argVec;
    for (int i = 0; i < argc; i++) {
        argVec.push_back(Glib::ustring(argv[i]));
    }

    return this->parse(argVec);
}

bool ArgParser::parse
            (std::vector<Glib::ustring> argVec) {
	bool retval = true;

	std::string marker("--");

	for (std::vector<Glib::ustring>::const_iterator i = argVec.begin() + 1; i != argVec.end(); i++) {
		std::string arg(*i);
		std::string key = arg;
		std::string value;

		if (key == "-h" || key == "--help") {
			received_args["help"];
			continue;
		}

		if (key.compare(0, marker.length(), marker) != 0) {
			// this is not an arg, append it
			// to the extra string
			if (extra_arg_str.length ()) {
				// it has stuff, we need to add blank space
				extra_arg_str += " ";
			}
			extra_arg_str += key;
			continue;
		}

		key = std::string(key, 2);	//trim marker

		std::string::size_type pos = key.find ('=');
		if (pos != std::string::npos) {
			key.erase (pos);
			value = std::string (arg, pos + 3);
		}

		std::map<std::string, Arg>::const_iterator iter;
		Arg current;

		if ((iter = expected_args.find (key)) == expected_args.end ()) {
			// ignore this argument and set the error string
			retval = false;
			error_str += _("Unexpected argument ") + key + "\n";
			continue;
		} else {
			current = iter->second;
		}

		if (current.get_has_arg () && !value.length ()) {
			// this expects an arg, but has received none.
			retval = false;
			error_str += key + _(" expects an argument.") + "\n";
		} else if (value.length () && !current.get_has_arg ()) {
			retval = false;
			error_str += key + _(" does not expect an argument.") + "\n";
		}

		if (conflicts (key)) {
			// use the previously supplied argument
			retval = false;
			error_str += key + _(" conflicts with another argument.") + "\n";
			continue;
		}

		// save it.
		received_args[key] = value;
	}

	return retval;
}

std::string ArgParser::help_text (void) const {
	std::string ret_str = _("Usage:");

	std::map<std::string, Arg>::const_iterator iter;

	for (iter=expected_args.begin();iter!=expected_args.end();iter++) {
		std::string name = "--" + iter->first;
		std::string arg = iter->second.get_has_arg () ? "=[arg]" : "";
		std::string desc = iter->second.get_desc ();
		ret_str += "\n\t" + name + arg + "\n\t\t" + desc;
	}

	return ret_str;
}

bool ArgParser::conflicts (std::string key) {
	std::vector< std::vector<std::string> >::const_iterator iter;

	// Look through the exclusive iter. Cramped together to fit
	// into 80 columns.
	for (iter=exclusive.begin();iter!=exclusive.end();iter++) {
		std::vector<std::string>::const_iterator str_iter;

		// Look through every option in this exclusive series.
		for (str_iter=iter->begin();str_iter!=iter->end();str_iter++) {
			if (*str_iter == key) {
				// this option cannot be used with any others
				// in this vector.
				// check if any of the options in this vector
				// have already been specified.
				std::vector<std::string>::const_iterator new_str_iter;
				for (new_str_iter=iter->begin();new_str_iter!=iter->end();new_str_iter++) {
					if (received_args.find (*new_str_iter) != received_args.end ()) {
						// this option conflicts with a
						// previous option.
						return true;
					}
				}
				// no conflicts.
				return false;
			}
		}
	}

	return false;
}
