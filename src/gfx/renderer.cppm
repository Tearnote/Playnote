module;
#include <utility>
#include "vuk/vsl/Core.hpp"
#include "vuk/Types.hpp"

export module playnote.gfx.renderer;

import playnote.sys.gpu;

namespace playnote::gfx {

using sys::ManagedImage;

export class Renderer {
public:
	explicit Renderer(sys::GPU& gpu): gpu{gpu} {}
	void draw();

private:
	sys::GPU& gpu;
};

void Renderer::draw()
{
	gpu.frame([](auto&, auto&& target) -> ManagedImage {
		return vuk::clear_image(std::move(target), vuk::ClearColor{0.655f, 0.639f, 0.741f, 1.0f});
	});
}

}
