/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

config.cpp:
Implementation file for config.hpp.
*/

#include "config.hpp"

#include <toml.hpp>
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
		auto& category = toml_data[entry.category];
		visit([&](auto const& v) { category[entry.name] = v; }, entry.value);
	}
	auto output = toml::format(toml::value{toml_data});
	io::write_file(ConfigPath, {reinterpret_cast<byte*>(output.data()), output.size()});
}

void Config::set_entry(Entry&& entry)
{
	//TODO
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
