module;

export module playnote.globals;

// Values available everywhere in the code. The approach to globals is to generally minimize them
// as much as possible, due to the following issues:
// - unclear destruction order
// - no compile-time assurance that the global is available when needed
// Low-level services are handled by a service locator. They should be completely self-contained
// and not use the locator themselves, instead depending on dependency injection.
// Game systems, on the other hand, will communicate via a central event queue. (TODO)

import playnote.util.locator;

namespace playnote {

// The global resource locator
export auto locator = util::Locator{};

}
