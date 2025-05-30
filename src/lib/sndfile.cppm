/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/sndfile.cppm:
Wrapper for libsndfile decoding.
*/

module;
#include "samplerate.h"
#include "sndfile.h"
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

auto resample_buffer(vector<pw::Sample> const& input, SF_INFO const& file_info, uint32 sampling_rate) -> vector<pw::Sample>
{
	auto const ratio = static_cast<double>(sampling_rate) / static_cast<double>(file_info.samplerate);
	auto output_frame_count = static_cast<long>(ceil(file_info.frames * ratio));
	output_frame_count = output_frame_count + 8 - (output_frame_count % 8); // Padding
	auto samples = vector<pw::Sample>{};
	samples.resize(output_frame_count);

	auto src_data = SRC_DATA{
		.data_in = reinterpret_cast<float const*>(input.data()),
		.data_out = reinterpret_cast<float*>(samples.data()),
		.input_frames = file_info.frames,
		.output_frames = output_frame_count,
		.src_ratio = ratio,
	};
	auto const ret = src_simple(&src_data, SRC_SINC_BEST_QUALITY, file_info.channels);
	if (ret != 0) throw runtime_error_fmt("Failed to resample audio: {}", src_strerror(ret));
	samples.resize(src_data.output_frames_gen * file_info.channels);
	return samples;
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
