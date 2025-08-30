/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

config.cpp:
Implementation file for config.hpp.
*/

#include "config.hpp"

#include <toml++/toml.hpp>
#include <sstream>
#include "preamble.hpp"
#include "logger.hpp"
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
	//TODO
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
	io::write_file("config.toml",
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

void Config::create_defaults()
{
	entries.emplace_back(Entry{
		.category = "audio",
		.name = "pipewire_buffer",
		.value = 128,
	});
	entries.emplace_back(Entry{
		.category = "audio",
		.name = "wasapi_exclusive",
		.value = true,
	});
}

}
