module;

export module playnote.gfx.renderer;

import playnote.util.service;
import playnote.sys.gpu;

namespace playnote::gfx {

class Renderer {
public:
	void draw();
};

void Renderer::draw()
{
	auto allocator = sys::s_gpu->begin_frame();
}

export auto s_renderer = util::Service<Renderer>{};

}
