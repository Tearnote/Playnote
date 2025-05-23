/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

gfx/renderer.cppm:
A renderer of primitives.
*/

export module playnote.gfx.renderer;

import playnote.preamble;
import playnote.sys.gpu;
import playnote.gfx.imgui;

namespace playnote::gfx {

using sys::ManagedImage;

export class Renderer {
public:
	// Solid color rectangle primitive
	struct Rect {
		ivec2 pos;
		ivec2 size;
		vec4 color;
	};

	// An accumulator of primitives to draw
	class Queue {
	public:
		// Add a solid color rectangle to the draw queue
		void enqueue_rect(Rect rect) { rects.emplace_back(rect); }

	private:
		friend Renderer;
		vector<Rect> rects{};
	};

	explicit Renderer(sys::GPU& gpu);

	// Provide a queue to the function argument, and then draw contents of the queue to the screen
	// Each call will wait block until the next frame begins
	// Imgui:: calls are only allowed within the function argument
	template<callable<void(Queue&)> Func>
	void frame(Func&&);

private:
	sys::GPU& gpu;
	gfx::Imgui imgui;

	// Drawing logic separated from frame() so that it's not templated
	void draw(Queue const&);
};

template<callable<void(Renderer::Queue&)> Func>
void Renderer::frame(Func&& func)
{
	auto queue = Queue{};
	imgui.enqueue([&]() { func(queue); });
	draw(queue);
}

}
