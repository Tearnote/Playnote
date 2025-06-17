/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/input_shouts.cppm:
Shouts that can be spawned by the input thread. Typically messages from the OS message queue.
*/

export module playnote.threads.input_shouts;

import playnote.preamble;

namespace playnote::threads {

export using ChartLoadRequest = fs::path;

}
