module;
#include <pipewire/pipewire.h>
#include "util/log_macros.hpp"
#include "config.hpp"

export module playnote.sys.audio;

namespace playnote::sys {

export class Audio {
public:
	Audio(int argc, char* argv[])
	{
		pw_init(&argc, &argv);
		L_DEBUG("Using libpipewire {}\n", pw_get_library_version());
	}
};

}
