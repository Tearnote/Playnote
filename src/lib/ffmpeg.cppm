/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/sndfile.cppm:
Wrapper for libsndfile decoding and libswresample resampling.
*/

module;
extern "C" {
#include <libswresample/swresample.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}
#include <cerrno>
#include "macros/assert.hpp"
#include "macros/logger.hpp"

export module playnote.lib.ffmpeg;

import playnote.preamble;
import playnote.logger;
import playnote.lib.pipewire;

namespace playnote::lib::ffmpeg {

auto check_ret(int ret) -> int
{
	if (ret < 0) throw runtime_error_fmt("ffmpeg error: {}", av_err2str(ret));
	return ret;
}

template<typename T>
auto check_ptr(T* ptr) -> T*
{
	if (!ptr) throw runtime_error_fmt("ffmpeg error: {}", av_err2str(errno));
	return ptr;
}

struct SeekBuffer {
	span<byte const> buffer;
	usize cursor;
};

auto av_io_read(void* opaque, uint8_t* buf, int buf_size) -> int
{
	auto& buffer = *static_cast<SeekBuffer*>(opaque);
	auto* byte_buf = reinterpret_cast<byte*>(buf);
	auto const bytes_available = buffer.buffer.size() - buffer.cursor;
	if (!bytes_available) return AVERROR_EOF;
	auto const bytes_to_read = buf_size < bytes_available ? buf_size : bytes_available;
	copy(buffer.buffer.subspan(buffer.cursor, bytes_to_read), byte_buf);
	buffer.cursor += bytes_to_read;
	return static_cast<int>(bytes_to_read);
}

auto av_io_write(void* opaque, uint8_t const* buf, int buf_size) -> int
{
	PANIC("ffmpeg attempted to write to a read-only file");
}

auto av_io_seek(void* opaque, int64_t offset, int whence) -> int64_t
{
	auto& buffer = *static_cast<SeekBuffer*>(opaque);
	auto new_cursor = 0zu;
	switch (whence) {
	case SEEK_SET: new_cursor = offset; break;
	case SEEK_CUR: new_cursor = buffer.cursor + offset; break;
	case SEEK_END: new_cursor = buffer.buffer.size() + offset; break;
	case AVSEEK_SIZE: return static_cast<int64_t>(buffer.buffer.size());
	default: PANIC("Unexpected ffmpeg seek mode");
	}

	if (new_cursor > buffer.buffer.size()) return -1;
	buffer.cursor = new_cursor;
	return static_cast<int64_t>(new_cursor);
}

struct DecoderOutput {
	vector<vector<byte>> data;
	usize sample_count;
	AVSampleFormat sample_format;
	uint32 sample_rate;
	AVChannelLayout channel_layout;
	bool planar;
};

auto decode_file_buffer(span<byte const> file_contents) -> DecoderOutput
{
	static constexpr auto PageSize = 4096zu;
	using AVBuffer = unique_resource<void*, decltype([](auto* buf) { av_free(buf); })>;
	using AVIO = unique_resource<AVIOContext*, decltype([](auto* ctx) {
		av_free(ctx->buffer);
		av_free(ctx);
	})>;
	using AVFormat = unique_resource<AVFormatContext*, decltype([](auto* ctx) {
		avformat_free_context(ctx);
	})>;
	using AVCodec = unique_resource<AVCodecContext*, decltype([](auto* ctx) {
		avcodec_free_context(&ctx);
	})>;

	auto file_buffer = SeekBuffer{ .buffer = file_contents, .cursor = 0 };
	auto io_buffer = AVBuffer{av_malloc(PageSize)};
	auto io = AVIO{check_ptr(avio_alloc_context(static_cast<unsigned char*>(io_buffer.get()), PageSize, 0,
		&file_buffer, &av_io_read, &av_io_write, &av_io_seek))};
	io_buffer.release(); // AVIOContext takes control over the buffer from now on
	auto format = AVFormat{check_ptr(avformat_alloc_context())};
	format->pb = io.get();

	auto* format_rw = format.get();
	format.release(); // avformat_open_input frees it on failure
	check_ret(avformat_open_input(&format_rw, "", nullptr, nullptr));
	format.reset(format_rw); // No failure; take back control

	check_ret(avformat_find_stream_info(format.get(), nullptr));
	auto stream_id = -1;
	for (auto const i: views::iota(0u, format->nb_streams)) {
		if (format->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			stream_id = i;
			break;
		}
	}
	if (stream_id == -1) throw runtime_error{"No audio stream found"};
	auto* stream = format->streams[stream_id];

	auto* codec = check_ptr(avcodec_find_decoder(stream->codecpar->codec_id));
	auto codec_ctx = AVCodec{check_ptr(avcodec_alloc_context3(codec))};
	check_ret(avcodec_parameters_to_context(codec_ctx.get(), stream->codecpar));
	codec_ctx->pkt_timebase = stream->time_base; // Fix "Could not update timestamps for discarded samples."
	check_ret(avcodec_open2(codec_ctx.get(), codec, nullptr));

	auto result = DecoderOutput{
		.sample_format = codec_ctx->sample_fmt,
		.sample_rate = static_cast<uint32>(codec_ctx->sample_rate),
		.channel_layout = codec_ctx->ch_layout,
		.planar = static_cast<bool>(av_sample_fmt_is_planar(codec_ctx->sample_fmt)),
	};
	auto const planes = result.planar? result.channel_layout.nb_channels : 1u;
	auto const bytes_per_sample = av_get_bytes_per_sample(codec_ctx->sample_fmt);
	auto const samples_per_frame = result.planar? 1u : result.channel_layout.nb_channels;
	auto const sample_count_estimate = stream->duration;
	auto byte_size_estimate = samples_per_frame * sample_count_estimate * bytes_per_sample;
	byte_size_estimate += 4096; // Overallocate slightly to avoid realloc on underestimation
	for (auto i: views::iota(0u, planes)) {
		result.data.emplace_back();
		result.data.back().resize(byte_size_estimate);
	}

	auto cursor = 0zu;
	auto packet = AVPacket{};
	auto frame = AVFrame{};
	auto flushing = false;
	while (!flushing) {
		auto const ret = av_read_frame(format.get(), &packet);
		if (ret == AVERROR_EOF)
			flushing = true;
		else
			check_ret(ret);
		check_ret(avcodec_send_packet(codec_ctx.get(), flushing? nullptr : &packet));
		av_packet_unref(&packet);

		while (true) {
			auto const ret = avcodec_receive_frame(codec_ctx.get(), &frame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
			check_ret(ret);

			auto const frame_bytes = frame.nb_samples * bytes_per_sample * samples_per_frame;
			if (cursor + frame_bytes > byte_size_estimate) {
				WARN("Underestimated output size by {} bytes", cursor + frame_bytes - byte_size_estimate);
				byte_size_estimate = cursor + frame_bytes;
				for (auto& vec: result.data)
					vec.resize(byte_size_estimate);
			}

			for (auto i: views::iota(0u, planes)) {
				ASSERT(frame.data[i]);
				copy(span{reinterpret_cast<byte*>(frame.data[i]), static_cast<usize>(frame_bytes)},
					result.data[i].begin() + cursor);
			}
			cursor += frame_bytes;
		}
	}

	ASSERT(cursor <= byte_size_estimate);
	for (auto& vec: result.data)
		vec.resize(cursor);
	ASSERT(cursor % bytes_per_sample == 0);
	result.sample_count = cursor / (bytes_per_sample * samples_per_frame);

	return result;
}

auto resample_buffer(DecoderOutput const& input, uint32 sampling_rate) -> vector<pw::Sample>
{
	auto* swr = static_cast<SwrContext*>(nullptr);
	constexpr auto channel_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
	check_ret(swr_alloc_set_opts2(&swr,
		&channel_layout, AV_SAMPLE_FMT_FLT, static_cast<int>(sampling_rate),
		&input.channel_layout, input.sample_format, static_cast<int>(input.sample_rate),
		0, nullptr));
	check_ret(av_opt_set_int(swr, "resampler", SWR_ENGINE_SOXR, 0));
	check_ret(swr_init(swr));

	auto max_out_samples = check_ret(swr_get_out_samples(swr, static_cast<int>(input.sample_count)));
	auto output = vector<pw::Sample>{};
	output.resize(max_out_samples);
	auto in_ptrs = vector<uint8_t const*>{};
	in_ptrs.reserve(input.data.size());
	for (auto const& vec: input.data)
		in_ptrs.push_back(reinterpret_cast<uint8_t const*>(vec.data()));
	auto* out_ptr = reinterpret_cast<uint8_t*>(output.data());
	auto out_samples = check_ret(swr_convert(swr, &out_ptr, output.size(), in_ptrs.data(), input.sample_count));
	out_samples += check_ret(swr_convert(swr, &out_ptr, output.size(), nullptr, 0)); // Flush final samples
	ASSERT(max_out_samples >= out_samples);
	output.resize(out_samples);

	swr_free(&swr);
	return output;
}

export auto decode_and_resample_file_buffer(span<byte const> file_contents, uint32 sampling_rate) -> vector<pw::Sample>
{
	auto decoder_output = decode_file_buffer(file_contents);
	auto result = resample_buffer(decoder_output, sampling_rate);
	return result;
}

}
