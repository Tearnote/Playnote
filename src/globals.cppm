/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

globals.cppm:
Values available everywhere in the code. The approach to globals is to generally minimize them
as much as possible, due to the following issues:
- unclear destruction order
- no compile-time assurance that the global is available when needed
Whenever suitable, dependency injection is preferred for dependency handling, and event callbacks
for inter-subsystem communication.
*/

module;
#include <optional>

export module playnote.globals;

import playnote.util.logger;

namespace playnote {

// The global logger
export auto g_logger = std::optional<util::Logger>{};

}
