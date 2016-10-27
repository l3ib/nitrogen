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

#ifndef _ARGPARSER_H_
#define _ARGPARSER_H_

#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <glibmm.h>

class Arg {
	public:
		Arg (	std::string desc = "",
			bool has_arg = false) {
			set_desc (desc);
			set_has_arg (has_arg);
		}

		std::string get_desc (void) const {
			return desc;
		}

		bool get_has_arg (void) const {
			return has_arg;
		}

		void set_desc (std::string desc) {
			this->desc = desc;
		}

		void set_has_arg (bool has_arg) {
			this->has_arg = has_arg;
		}

		~Arg () {};

	protected:
		std::string desc;
		bool has_arg;
};

class ArgParser {
	public:
		ArgParser (void) {
			register_option ("help, -h", "Prints this help text.");
		};
		~ArgParser () {};

		// adds a valid option.
		inline void register_option (std::string name, std::string desc = "", bool has_arg = false) {
			expected_args[name] = Arg(desc, has_arg);
		}

		// makes two or more options mutually exclusive (only one or the other.)
		inline void make_exclusive (std::vector<std::string> exclusive_opts) {
			exclusive.push_back (exclusive_opts);
		}

		// returns the parsed map.
		inline std::map<std::string, std::string> get_map (void) const {
			return received_args;
		}

		// returns all errors.
		inline std::string get_error (void) const {
			return error_str;
		}

		inline std::string get_extra_args (void) const {
			return extra_arg_str;
		}

		inline bool has_argument (std::string option) {
			if (received_args.find (option) != received_args.end ()) {
				return true;
			}
			return false;
		}

		inline std::string get_value (std::string key) {
			std::map<std::string, std::string>::const_iterator iter = received_args.find (key);
			if (iter != received_args.end ()) {
				// key exists.
				return iter->second;
			}

			// key doesn't exist.
			return std::string ();
		}

                inline int get_intvalue (std::string key) {
                        std::stringstream stream;
                        std::string tmp_str = this->get_value(key);
                        int tmp_int;

                        stream << tmp_str;
                        stream >> tmp_int;
                        return tmp_int;
                }

		// parses away.
		bool parse (int argc, char ** argv);
        bool parse (std::vector<Glib::ustring> argVec);

		// returns the help text (--help)
		std::string help_text (void) const;

	protected:
		// checks if the supplied key conflicts with an existing key.
		bool conflicts (std::string key);

		std::string error_str, extra_arg_str;
		std::map<std::string, std::string> received_args;
		std::map<std::string, Arg> expected_args;
		std::vector< std::vector<std::string> > exclusive;
};

#endif
