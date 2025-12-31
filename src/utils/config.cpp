/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "utils/config.hpp"

#include <toml++/toml.hpp>
#include <sstream>
#include "preamble.hpp"
#include "utils/logger.hpp"
#include "utils/assert.hpp"
#include "io/file.hpp"

namespace playnote {

Config::~Config() noexcept
try {
	save_to_file();
} catch (exception const& e) {
	ERROR("Failed to flush config to file: {}", e.what());
}

void Config::load_from_file()
{
	if (!fs::exists(ConfigPath)) return;
	auto const file = io::read_file(ConfigPath);
	auto const toml_data = toml::parse(
		{reinterpret_cast<char const*>(file.contents.data()), file.contents.size()});

	for (auto& entry: entries) {
		if (!toml_data.contains(entry.category)) continue;
		auto const& category_table = *toml_data[entry.category].as_table();
		if (!category_table.contains(entry.name)) continue;
		visit([&](auto& v) {
			auto const& toml_entry = category_table[entry.name].value<remove_cvref_t<decltype(v)>>();
			if (!toml_entry) return;
			v = *toml_entry;
		}, entry.value);
	}
}

void Config::save_to_file() const
{
	auto toml_data = toml::table{};
	for (auto const& entry: entries) {
		if (!toml_data.contains(entry.category))
			toml_data.insert(entry.category, toml::table{});
		auto& category_table = *toml_data[entry.category].as_table();
		visit([&](auto const& v) { category_table.insert_or_assign(entry.name, v); }, entry.value);
	}

	auto file_content = std::stringstream{};
	file_content << toml_data;
	auto file_content_view = file_content.view();
	io::write_file(ConfigPath,
		{reinterpret_cast<byte const*>(file_content_view.data()), file_content_view.size()});
}

void Config::set_entry(Entry&& entry)
{
	find_entry(entry.category, entry.name).value = move(entry.value);
}

auto Config::find_entry(string_view category, string_view name) -> Entry&
{
	auto iter = find_if(entries, [&](auto const& e) { return e.category == category && e.name == name; });
	ASSERT(iter != entries.end());
	return *iter;
}

auto Config::find_entry(string_view category, string_view name) const -> Entry const&
{
	auto iter = find_if(entries, [&](auto const& e) { return e.category == category && e.name == name; });
	ASSERT(iter != entries.end());
	return *iter;
}

// Consult this function for the list of registered config entries.
void Config::create_defaults()
{
	entries.emplace_back(Entry{
		.category = "system",
		.name = "attach_console",
		.value = false,
	});

	entries.emplace_back(Entry{
		.category = "logging",
		.name = "global",
		.value = "Info",
	});
	entries.emplace_back(Entry{
		.category = "logging",
		.name = "input",
		.value = "Info",
	});
	entries.emplace_back(Entry{
		.category = "logging",
		.name = "render",
		.value = "Info",
	});
	entries.emplace_back(Entry{
		.category = "logging",
		.name = "audio",
		.value = "Info",
	});
	entries.emplace_back(Entry{
		.category = "logging",
		.name = "library",
		.value = "Info",
	});

	entries.emplace_back(Entry{
		.category = "pipewire",
		.name = "buffer_size",
		.value = 128,
	});

	entries.emplace_back(Entry{
		.category = "wasapi",
		.name = "exclusive_mode",
		.value = true,
	});
	entries.emplace_back(Entry{
		.category = "wasapi",
		.name = "use_custom_latency",
		.value = false,
	});
	entries.emplace_back(Entry{
		.category = "wasapi",
		.name = "custom_latency",
		.value = 10,
	});

	entries.emplace_back(Entry{
		.category = "graphics",
		.name = "swapchain_image_count",
		.value = 2,
	});
	entries.emplace_back(Entry{
		.category = "graphics",
		.name = "low_latency",
		.value = true,
	});
	entries.emplace_back(Entry{
		.category = "graphics",
		.name = "validation_enabled",
		.value = false,
	});
	entries.emplace_back(Entry{
		.category = "graphics",
		.name = "subpixel_layout_override",
		.value = "",
	});

	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_5k_1",
		.value = "Z",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_5k_2",
		.value = "S",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_5k_3",
		.value = "X",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_5k_4",
		.value = "D",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_5k_5",
		.value = "C",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_5k_s",
		.value = "LeftShift",
	});

	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_7k_1",
		.value = "Z",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_7k_2",
		.value = "S",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_7k_3",
		.value = "X",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_7k_4",
		.value = "D",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_7k_5",
		.value = "C",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_7k_6",
		.value = "F",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_7k_7",
		.value = "V",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_7k_s",
		.value = "LeftShift",
	});

	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p1_1",
		.value = "Z",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p1_2",
		.value = "S",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p1_3",
		.value = "X",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p1_4",
		.value = "D",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p1_5",
		.value = "C",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p1_s",
		.value = "LeftShift",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p2_1",
		.value = "M",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p2_2",
		.value = "K",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p2_3",
		.value = "Comma",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p2_4",
		.value = "L",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p2_5",
		.value = "Period",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_10k_p2_s",
		.value = "RightShift",
	});

	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p1_1",
		.value = "Z",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p1_2",
		.value = "S",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p1_3",
		.value = "X",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p1_4",
		.value = "D",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p1_5",
		.value = "C",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p1_6",
		.value = "F",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p1_7",
		.value = "V",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p1_s",
		.value = "LeftShift",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p2_1",
		.value = "M",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p2_2",
		.value = "K",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p2_3",
		.value = "Comma",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p2_4",
		.value = "L",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p2_5",
		.value = "Period",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p2_6",
		.value = "Semicolon",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p2_7",
		.value = "Slash",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "kb_14k_p2_s",
		.value = "RightShift",
	});

	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_5k_1",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_5k_2",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_5k_3",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_5k_4",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_5k_5",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_5k_s",
		.value = "None",
	});

	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_7k_1",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_7k_2",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_7k_3",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_7k_4",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_7k_5",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_7k_6",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_7k_7",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_7k_s",
		.value = "None",
	});

	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p1_1",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p1_2",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p1_3",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p1_4",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p1_5",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p1_s",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p2_1",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p2_2",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p2_3",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p2_4",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p2_5",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p2_s",
		.value = "None",
	});

	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p1_1",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p1_2",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p1_3",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p1_4",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p1_5",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p1_6",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p1_7",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p1_s",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p2_1",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p2_2",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p2_3",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p2_4",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p2_5",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p2_6",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p2_7",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p2_s",
		.value = "None",
	});

	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_5k_s_analog",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_7k_s_analog",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p1_s_analog",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_10k_p2_s_analog",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p1_s_analog",
		.value = "None",
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "con_14k_p2_s_analog",
		.value = "None",
	});

	entries.emplace_back(Entry{
		.category = "controls",
		.name = "debounce_duration",
		.value = 4,
	});
	entries.emplace_back(Entry{
		.category = "controls",
		.name = "turntable_stop_timeout",
		.value = 200,
	});

	entries.emplace_back(Entry{
		.category = "gameplay",
		.name = "scroll_speed",
		.value = 3.0,
	});
	entries.emplace_back(Entry{
		.category = "gameplay",
		.name = "note_offset",
		.value = 0,
	});
	entries.emplace_back(Entry{
		.category = "gameplay",
		.name = "judgment_timeout",
		.value = 400,
	});
}

}
