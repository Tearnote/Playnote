/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "lib/imgui.hpp"
#include "lib/vuk.hpp"
#include "dev/gpu.hpp"
#include "utils/assets.hpp"

namespace playnote::gfx {

// Encapsulation of Dear ImGui initialization and drawing.
class Imgui {
public:
	explicit Imgui(dev::GPU& gpu):
		context{lib::imgui::init(gpu.get_window().handle(), gpu.get_global_allocator(),
			playnote::globals::assets->get("unifont-16.0.04.ttf"_id))}
	{}

	// Prepare Imgui to accept commands.
	// All imgui:: functions must be run within the provided function.
	template<callable<void()> Func>
	void enqueue(Func);

	// Draw enqueued Imgui state into the image. Must be run once and after enqueue().
	auto draw(lib::vuk::Allocator&, dev::ManagedImage) -> dev::ManagedImage;

private:
	InstanceLimit<Imgui, 1> instance_limit;
	lib::imgui::Context context;
};

template<callable<void()> Func>
void Imgui::enqueue(Func func)
{
	lib::imgui::begin();
	func();
	lib::imgui::end();
}

inline auto Imgui::draw(lib::vuk::Allocator& allocator, dev::ManagedImage target) -> dev::ManagedImage
{
	return lib::imgui::render(allocator, move(target), context);
}

}
