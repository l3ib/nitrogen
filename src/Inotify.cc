#include "main.h"

/* don't compile if the option is not set */
#ifdef USE_INOTIFY

#include "Inotify.h"

#include <sys/inotify.h>

#include <errno.h>
#include <sys/select.h>

#define MAXLEN 16384

namespace Inotify {

std::map< int, Watch* > Watch::watch_descriptors;
int Watch::inotify_fd = -1;

Watch * Watch::create(std::string path) {
	// check inotify_fd; set it if it is currently unset
	if (inotify_fd == -1) {
		inotify_fd = inotify_init();
		if (inotify_fd == -1) {
			// an error occurred.
			switch(errno) { // oh dear god.
				case EMFILE:
					break;
				case ENFILE:
					break;
				case ENOMEM:
					break;
			}
			return 0;
		}
	}
	Watch * retval = new Watch(path);
	int watch_descriptor = -1;
	if ((watch_descriptor = inotify_add_watch(inotify_fd, path.c_str(),
		IN_ALL_EVENTS)) == -1) {
		return 0;
	}
	watch_descriptors[watch_descriptor] = retval;
	retval->watch_descriptor = watch_descriptor;

	retval->signal_deleted_self.connect(sigc::mem_fun(retval, &Watch::remove_wrap));

	return retval;
}

void Watch::poll(suseconds_t timeout) {
	if (inotify_fd == -1) return;
	// initialize the fd set for select()
	fd_set set;
	FD_ZERO(&set);
	FD_SET(inotify_fd, &set);

	// initialize the timeout
	struct timeval timeval;
	timeval.tv_sec = 0;
	timeval.tv_usec = timeout;

	int select_return = -1;

	select_return = select(inotify_fd + 1, &set, NULL, NULL, &timeval);
	if (select_return <= 0) return;
	// something interesting happened.

	char buffer[MAXLEN];
	ssize_t size = 0;
	size = read(inotify_fd, buffer, MAXLEN);
	if (size <= 0) return;

	// process events
	ssize_t i = 0;
	while(i < size) {
		uint32_t wd = *((uint32_t*)&buffer[i]);
		i += sizeof(uint32_t);
		uint32_t mask = *((uint32_t*)&buffer[i]);
		i += sizeof(uint32_t);
		//uint32_t cookie = buffer[i];
		i += sizeof(uint32_t);
		uint32_t len = *((uint32_t*)&buffer[i]);
		i += sizeof(uint32_t);
		std::string name(&buffer[i], len);
		i += len;

		if (watch_descriptors.find(wd) == watch_descriptors.end()) continue;

		Watch * watch = watch_descriptors[wd];

		std::string path;
		if (name.length() == 0) {
			name = watch->path;
		} else {
			name = watch->path + "/" + name;
		}

		// you have absolutely no idea how much of a pita it was writing this
		// out.
		if (mask & IN_ACCESS) {
			watch->signal_accessed.emit(name);
		}
		if (mask & IN_ATTRIB) {
			watch->signal_metadata_changed.emit(name);
		}
		if (mask & IN_CLOSE_WRITE) {
			watch->signal_write_closed.emit(name);
		}
		if (mask & IN_CLOSE_NOWRITE) {
			watch->signal_nowrite_closed.emit(name);
		}
		if (mask & IN_CREATE) {
			watch->signal_created.emit(name);
		}
		if (mask & IN_DELETE) {
			watch->signal_deleted.emit(name);
		}
		if (mask & IN_DELETE_SELF) {
			watch->signal_deleted_self.emit(name);
		}
		if (mask & IN_MODIFY) {
			watch->signal_modified.emit(name);
		}
		/*if (mask & IN_MOVE_SELF) {
			watch->signal_moved_self.emit(name);
		}*/ // not supported in my "inotify.h" glue
		if (mask & IN_MOVED_FROM) {
			watch->signal_moved_from.emit(name);
		}
		if (mask & IN_MOVED_TO) {
			watch->signal_moved_to.emit(name);
		}
		if (mask & IN_OPEN) {
				watch->signal_opened.emit(name);
		}
	}
}

void Watch::remove(void) {
	inotify_rm_watch(inotify_fd, watch_descriptor);
	watch_descriptors.erase(watch_descriptor);
	delete this; // self aware!
}

}

#endif /* USE_INOTIFY */
