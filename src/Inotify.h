#include "main.h"

/* don't compile if the option is not set */
#ifdef USE_INOTIFY

#include <sigc++/sigc++.h>
#include <stdexcept>
#include <string>
#include <map>
#include <sys/time.h>

namespace Inotify {

// the string here is the path to the file that the signal is for
typedef sigc::signal<void, std::string> inotify_signal;

class Watch {
	public:
		// returns an inotify 'watch' associated with 'path'
		// internally calls inotify_add_watch() and adds itself to the map of
		// watch descriptors.
		static Watch * create(std::string path);

		// stops monitoring a file or directory; calls inotify_rm_watch()
		void remove(void);

		// should be called on a timeout or something; calls select() and waits
		// for events. timeout is the amount of time to wait in microseconds.
		static void poll(suseconds_t timeout);

		// signals that can be emitted
		inotify_signal signal_accessed; // file was accessed
		inotify_signal signal_metadata_changed; // file permissions.. changed
		inotify_signal signal_write_closed; // file opened for writing closed
		inotify_signal signal_nowrite_closed; // file opened for reading closed
		inotify_signal signal_created; // file created in watched dir
		inotify_signal signal_deleted; // file deleted in watched dir
		inotify_signal signal_deleted_self; // file/dir itself deleted
		inotify_signal signal_modified; // file modified
		inotify_signal signal_moved_self; // self was moved
		inotify_signal signal_moved_from; // file was moved out
		inotify_signal signal_moved_to; // file was moved in
		inotify_signal signal_opened; // file opened

	protected:
		Watch(std::string path) {
			this->path = path;
			watch_descriptor = -1;
		};
		~Watch() {};

		void remove_wrap(std::string path) {
			remove();
		}

		// maps watch descriptor -> inotify object
		static std::map< int, Watch* > watch_descriptors;
		// inotify file descriptor returned by inotify_init()
		static int inotify_fd;

		// the path that this watch is for
		std::string path;

		// this watch's descriptor
		int watch_descriptor;
};

}

#endif /* USE_INOTIFY */
