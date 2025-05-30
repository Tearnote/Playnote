/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/sndfile.cppm:
Wrapper for libsndfile decoding and libswresample resampling.
*/

module;
extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}
#include <sndfile.h>
#include "macros/assert.hpp"
#include "macros/logger.hpp"

export module playnote.lib.sndfile;

import playnote.preamble;
import playnote.logger;
import playnote.lib.pipewire;

namespace playnote::lib::sndfile {

struct SeekBuffer {
	span<byte const> buffer;
	usize cursor;
};

auto io_api_length(void* user_data) noexcept -> sf_count_t
{
	auto const& buffer = *static_cast<SeekBuffer*>(user_data);
	return buffer.buffer.size();
}

auto io_api_seek(sf_count_t offset, int whence, void* user_data) noexcept -> sf_count_t
{
	auto& buffer = *static_cast<SeekBuffer*>(user_data);
	auto new_cursor = 0zu;
	switch (whence) {
	case SEEK_CUR: new_cursor = buffer.cursor + offset; break;
	case SEEK_SET: new_cursor = offset; break;
	case SEEK_END: new_cursor = buffer.buffer.size() + offset; break;
	default: PANIC("Unexpected libsndfile seek mode");
	}

	if (new_cursor > buffer.buffer.size()) return -1;
	buffer.cursor = new_cursor;
	return new_cursor;
}

auto io_api_read(void* ptr, sf_count_t count, void* user_data) noexcept -> sf_count_t
{
	auto& buffer = *static_cast<SeekBuffer*>(user_data);
	auto* byte_ptr = static_cast<byte*>(ptr);
	auto const bytes_available = buffer.buffer.size() - buffer.cursor;
	auto const bytes_to_read = count < bytes_available ? count : bytes_available;
	copy(buffer.buffer.subspan(buffer.cursor, bytes_to_read), byte_ptr);
	buffer.cursor += bytes_to_read;
	return bytes_to_read;
}

auto io_api_write(void const* ptr, sf_count_t count, void* user_data) -> sf_count_t
{
	PANIC("libsndfile attempted to write to a read-only file");
}

auto io_api_tell(void* user_data) noexcept -> sf_count_t
{
	auto const& buffer = *static_cast<SeekBuffer*>(user_data);
	return buffer.cursor;
}

auto decode_file_buffer(span<byte const> file_contents) -> pair<vector<pw::Sample>, SF_INFO>
{
	using SndFile = unique_resource<SNDFILE*, decltype([](auto* f) {
		sf_close(f);
	})>;

	static auto io_api = SF_VIRTUAL_IO{
		.get_filelen = &io_api_length,
		.seek = &io_api_seek,
		.read = &io_api_read,
		.write = &io_api_write,
		.tell = &io_api_tell,
	};
	auto buffer = SeekBuffer{
		.buffer = file_contents,
		.cursor = 0,
	};
	auto file_info = SF_INFO{};
	auto sndfile = SndFile{sf_open_virtual(&io_api, SFM_READ, &file_info, &buffer)};
	if (!sndfile.get()) throw runtime_error_fmt("Failed to open sound file: {}", sf_strerror(nullptr));
	ASSERT(file_info.channels == 2);
	static_assert(sizeof(pw::Sample) == sizeof(float) * 2);

	auto samples = vector<pw::Sample>{};
	samples.resize(file_info.frames);
	sf_readf_float(sndfile.get(), reinterpret_cast<float*>(samples.data()), file_info.frames);
	if (sf_error(sndfile.get()) != 0)
		throw runtime_error_fmt("Failed to read sound file: {}", sf_strerror(sndfile.get()));

	return make_pair(move(samples), file_info);
}

auto check_ret(int ret) -> int
{
	if (ret < 0) throw runtime_error_fmt("ffmpeg error: {}", av_err2str(ret));
	return ret;
}

auto resample_buffer(vector<pw::Sample> const& input, SF_INFO const& file_info, uint32 sampling_rate) -> vector<pw::Sample>
{
	auto* swr = static_cast<SwrContext*>(nullptr);
	constexpr auto channel_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
	check_ret(swr_alloc_set_opts2(&swr,
		&channel_layout, AV_SAMPLE_FMT_FLT, sampling_rate,
		&channel_layout, AV_SAMPLE_FMT_FLT, file_info.samplerate,
		0, nullptr));
	check_ret(av_opt_set_int(swr, "resampler", SWR_ENGINE_SOXR, 0));
	check_ret(swr_init(swr));

	auto max_out_samples = check_ret(swr_get_out_samples(swr, file_info.frames));
	auto output = vector<pw::Sample>{};
	output.resize(max_out_samples);
	auto* in_ptr = reinterpret_cast<uint8_t const*>(input.data());
	auto* out_ptr = reinterpret_cast<uint8_t*>(output.data());
	auto out_samples = check_ret(swr_convert(swr, &out_ptr, output.size(), &in_ptr, input.size()));
	out_samples += check_ret(swr_convert(swr, &out_ptr, output.size(), nullptr, 0)); // Flush final samples
	ASSERT(max_out_samples >= out_samples);
	output.resize(out_samples);

	swr_free(&swr);
	return output;
}

export auto decode_and_resample_file_buffer(span<byte const> file_contents, uint32 sampling_rate) -> vector<pw::Sample>
{
	auto [samples, file_info] = decode_file_buffer(file_contents);
	TRACE("Decoded");
	if (file_info.samplerate == sampling_rate) return samples;
	auto result = resample_buffer(samples, file_info, sampling_rate);
	TRACE("Resampled");
	return result;
}

}
