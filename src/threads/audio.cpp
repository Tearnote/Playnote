/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/audio.cpp:
Implementation file for threads/audio.hpp.
*/

#include "threads/audio.hpp"

#include "preamble.hpp"
#include "assert.hpp"
#include "logger.hpp"
#include "dev/window.hpp"
#include "dev/os.hpp"
#include "audio/player.hpp"
#include "audio/mixer.hpp"
#include "bms/library.hpp"
#include "bms/input.hpp"
#include "threads/render_shouts.hpp"
#include "threads/input_shouts.hpp"
#include "threads/broadcaster.hpp"

namespace playnote::threads {

static void run_audio(Broadcaster& broadcaster, dev::Window& window, audio::Mixer& mixer)
{
	auto library = bms::Library{LibraryDBPath};
	auto mapper = bms::Mapper{};
	auto player = make_shared<audio::Player>(window.get_glfw(), mixer);

	while (!window.is_closing()) {
		broadcaster.receive_all<LibraryRefreshRequest>([&](auto) {
			broadcaster.shout(library.list_charts());
		});
		broadcaster.receive_all<LoadChart>([&](auto ev) {
			auto chart = library.load_chart(ev);
			player->play(*chart, false);
		});
		broadcaster.receive_all<PlayerControl>([&](auto ev) {
			switch (ev) {
			case PlayerControl::Play:
				player->resume();
				break;
			case PlayerControl::Pause:
				player->pause();
				break;
			case PlayerControl::Restart:
				player->play(player->get_chart(), false);
				break;
			case PlayerControl::Autoplay:
				player->play(player->get_chart(), true);
				break;
			default: PANIC();
			}
		});
		broadcaster.receive_all<KeyInput>([&](auto ev) {
			if (!player->is_playing()) return;
			auto input = mapper.from_key(ev, player->get_chart().metadata.playstyle);
			if (!input) return;
			player->enqueue_input(*input);
		});
		broadcaster.receive_all<ButtonInput>([&](auto ev) {
			if (!player->is_playing()) return;
			auto input = mapper.from_button(ev, player->get_chart().metadata.playstyle);
			if (!input) return;
			player->enqueue_input(*input);
		});
		broadcaster.receive_all<AxisInput>([&](auto ev) {
			if (!player->is_playing()) return;
			auto inputs = mapper.submit_axis_input(ev, player->get_chart().metadata.playstyle);
			for (auto const& input: inputs) player->enqueue_input(input);
		});
		broadcaster.receive_all<FileDrop>([&](auto ev) {
			for (auto const& path: ev.paths) library.import(path);
		});
		if (player->is_playing()) {
			auto inputs = mapper.from_axis_state(window.get_glfw(), player->get_chart().metadata.playstyle);
			for (auto const& input: inputs) player->enqueue_input(input);
		}

		yield();
	}
}

void audio(Broadcaster& broadcaster, Barriers<3>& barriers, dev::Window& window)
try {
	dev::name_current_thread("audio");
	broadcaster.register_as_endpoint();
	broadcaster.subscribe<LibraryRefreshRequest>();
	broadcaster.subscribe<LoadChart>();
	broadcaster.subscribe<PlayerControl>();
	broadcaster.subscribe<KeyInput>();
	broadcaster.subscribe<ButtonInput>();
	broadcaster.subscribe<AxisInput>();
	broadcaster.subscribe<FileDrop>();
	barriers.startup.arrive_and_wait();
	auto mixer = audio::Mixer{};
	run_audio(broadcaster, window, mixer);
	barriers.shutdown.arrive_and_wait();
}
catch (exception const& e) {
	CRIT("Uncaught exception: {}", e.what());
	window.request_close();
	barriers.shutdown.arrive_and_wait();
}

}
