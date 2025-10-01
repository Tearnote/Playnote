/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/build.hpp:
Parsing of BMS chart data into a Chart object.
*/

#pragma once
#include "preamble.hpp"
#include "io/song.hpp"
#include "bms/chart.hpp"

namespace playnote::bms {

inline auto chart_from_bms(io::Song& song, span<byte const> chart_raw, optional<reference_wrapper<Metadata>> cache = nullopt) -> shared_ptr<Chart const>
{
	auto chart = make_shared<Chart>();
	chart->md5 = lib::openssl::md5(chart_raw);
	if (cache) chart->metadata = *cache;
	chart->metadata.density.resolution = 1ms;
	return chart;
}

}
