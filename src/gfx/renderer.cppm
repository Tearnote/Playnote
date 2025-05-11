module;
#include <vector>

export module playnote.gfx.renderer;

import playnote.stx.callable;
import playnote.stx.types;
import playnote.stx.math;
import playnote.sys.gpu;
import playnote.gfx.imgui;

namespace playnote::gfx {

using stx::ivec2;
using stx::vec4;
using stx::uint;
using sys::ManagedImage;

export class Renderer {
public:
	struct Rect {
		ivec2 pos;
		ivec2 size;
		vec4 color;
	};

	class Queue {
	public:
		// Add a solid color rectangle to the draw queue
		void enqueue_rect(Rect rect) { rects.emplace_back(rect); }

	private:
		friend Renderer;
		std::vector<Rect> rects{};
	};

	explicit Renderer(sys::GPU& gpu);

	template<stx::callable<void(Queue&)> Func>
	void frame(Func&&);

private:
	sys::GPU& gpu;
	gfx::Imgui imgui;

	void draw(Queue const&);
};

template<stx::callable<void(Renderer::Queue&)> Func>
void Renderer::frame(Func&& func)
{
	auto queue = Queue{};
	imgui.enqueue([&]() { func(queue); });
	draw(queue);
}

}
