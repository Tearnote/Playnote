module;
#include <utility>
#include "vuk/vsl/Core.hpp"
#include "vuk/Types.hpp"

export module playnote.gfx.renderer;

import playnote.util.service;
import playnote.sys.gpu;

namespace playnote::gfx {

using sys::ManagedImage;
using sys::s_gpu;

class Renderer {
public:
	void draw();
};

void Renderer::draw()
{
	s_gpu->frame([](auto&, auto&& target) -> ManagedImage {
		return vuk::clear_image(std::move(target), vuk::ClearColor{0.655f, 0.639f, 0.741f, 1.0f});
	});
}

export auto s_renderer = util::Service<Renderer>{};

}
