/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

config.cpp:
Implementation file for config.hpp.
*/

#include "config.hpp"

#include "preamble.hpp"

namespace playnote {

Config::~Config() noexcept
{
	//TODO
}

void Config::load_from_file()
{
	//TODO
}

void Config::save_to_file() const
{
	//TODO
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
