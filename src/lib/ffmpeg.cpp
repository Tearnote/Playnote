/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "lib/ffmpeg.hpp"

extern "C" {
#include <libswresample/swresample.h>
#include <libavformat/version_major.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}
#include <cstdarg>
#include <cerrno>
#include <cstdio>
#include "preamble.hpp"
#include "utils/assert.hpp"
#include "utils/logger.hpp"
#include "lib/audio_common.hpp"

namespace playnote::lib::ffmpeg {

// Fix av_err2str macro making use of C-only features
#ifdef av_err2str
#undef av_err2str
av_always_inline auto av_err2string(int errnum) -> string {
	char str[AV_ERROR_MAX_STRING_SIZE];
	return av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, errnum);
}
#define av_err2str(err) av_err2string(err).c_str()
#endif

struct DecoderOutput_t {
	vector<vector<byte>> data;
	ssize_t sample_count;
	AVSampleFormat sample_format;
	int sample_rate;
	AVChannelLayout channel_layout;
	bool planar;
};

// Helper functions for error handling

static auto ret_check(int ret) -> int
{
	if (ret < 0) throw runtime_error_fmt("ffmpeg error: {}", av_err2str(ret));
	return ret;
}

template<typename T>
static auto ptr_check(T* ptr) -> T*
{
	if (!ptr) throw runtime_error_fmt("ffmpeg error: {}", av_err2str(errno));
	return ptr;
}

// Logger override support

thread_local Logger::Category cat = nullptr;
static atomic<bool> log_callback_set = false;

static void log_callback(void*, int level, char const* fmt, va_list va_og)
{
	if (level > 32) return; // Too verbose

	va_list va; // not auto due to possibly-macro shenanigans
	va_copy(va, va_og);
	auto const msg_size = std::vsnprintf(nullptr, 0, fmt, va);
	va_end(va);
	if (msg_size < 0) return;
	auto msg = string(msg_size, '\0'); // uniform initializer misinterprets this as a list of chars
	std::vsnprintf(msg.data(), msg_size + 1, fmt, va_og);

	auto log_cat = cat? cat : globals::logger->global;
	     if (level <=  8)  CRIT_AS(log_cat, "ffmpeg: {}", msg);
	else if (level <= 16) ERROR_AS(log_cat, "ffmpeg: {}", msg);
	else if (level <= 24)  WARN_AS(log_cat, "ffmpeg: {}", msg);
	else                   INFO_AS(log_cat, "ffmpeg: {}", msg);
}

static void set_log_callback()
{
	auto prev = log_callback_set.exchange(true);
	if (prev) return;
	av_log_set_callback(log_callback);
}

// RAII wrappers for ffmpeg objects

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
using AVPacket = unique_resource<AVPacket*, decltype([](auto* p) {
	av_packet_free(&p);
})>;
using AVFrame = unique_resource<AVFrame*, decltype([](auto* f) {
	av_frame_free(&f);
})>;

// Data buffer wrapper with a cursor for seeking support.
struct SeekBuffer {
	span<byte const> buffer;
	ssize_t cursor;
};

// Memory buffer IO callbacks

static auto av_io_read(void* opaque, uint8_t* buf, int buf_size) -> int
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

static auto av_io_seek(void* opaque, int64_t offset, int whence) -> int64_t
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

// The write callback uses a vector<byte> rather than SeekBuffer!
// However, there should be no overlap between read and write callbacks.
#if LIBAVFORMAT_VERSION_MAJOR < 61
static auto av_io_write(void* opaque, uint8_t* buf, int buf_size) -> int
#else
static auto av_io_write(void* opaque, uint8_t const* buf, int buf_size) -> int
#endif
{
	auto& out_buf = *static_cast<vector<byte>*>(opaque);
	auto const* in_buf = reinterpret_cast<byte const*>(buf);
	out_buf.reserve(out_buf.size() + buf_size);
	copy(span{in_buf, static_cast<size_t>(buf_size)}, back_inserter(out_buf));
	return buf_size;
}

void set_thread_log_category(Logger::Category new_cat)
{
	cat = new_cat;
}

auto decode_file_buffer(span<byte const> file_contents) -> DecoderOutput
{
	set_log_callback();
	auto file_buffer = SeekBuffer{ .buffer = file_contents, .cursor = 0 };
	auto io_buffer = AVBuffer{av_malloc(PageSize)};
	auto io = AVIO{ptr_check(avio_alloc_context(static_cast<unsigned char*>(io_buffer.get()), PageSize, 0,
		&file_buffer, &av_io_read, nullptr, &av_io_seek))};
	io_buffer.release(); // AVIOContext takes control over the buffer from now on
	auto format = AVFormat{ptr_check(avformat_alloc_context())};
	format->pb = io.get();

	auto* format_rw = format.get();
	format.release(); // avformat_open_input frees it on failure
	ret_check(avformat_open_input(&format_rw, "", nullptr, nullptr));
	format.reset(format_rw); // No failure; take back control

	ret_check(avformat_find_stream_info(format.get(), nullptr));
	auto stream_id = -1;
	for (auto const i: views::iota(0u, format->nb_streams)) {
		if (format->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			stream_id = i;
			break;
		}
	}
	if (stream_id == -1) throw runtime_error{"No audio stream found"};
	auto* stream = format->streams[stream_id];

	auto* codec = ptr_check(avcodec_find_decoder(stream->codecpar->codec_id));
	auto codec_ctx = AVCodec{ptr_check(avcodec_alloc_context3(codec))};
	ret_check(avcodec_parameters_to_context(codec_ctx.get(), stream->codecpar));
	codec_ctx->pkt_timebase = stream->time_base; // Fix "Could not update timestamps for discarded samples."
	ret_check(avcodec_open2(codec_ctx.get(), codec, nullptr));

	auto result = new DecoderOutput_t{
		.sample_format = codec_ctx->sample_fmt,
		.sample_rate = codec_ctx->sample_rate,
		.channel_layout = codec_ctx->ch_layout,
		.planar = static_cast<bool>(av_sample_fmt_is_planar(codec_ctx->sample_fmt)),
	};
	auto const planes = result->planar? result->channel_layout.nb_channels : 1u;
	auto const bytes_per_sample = av_get_bytes_per_sample(codec_ctx->sample_fmt);
	auto const samples_per_frame = result->planar? 1u : result->channel_layout.nb_channels;
	auto const sample_count_estimate = stream->duration;
	auto byte_size_estimate = samples_per_frame * sample_count_estimate * bytes_per_sample;
	byte_size_estimate += 4096; // Overallocate slightly to avoid realloc on underestimation
	for (auto i: views::iota(0u, planes)) {
		result->data.emplace_back();
		result->data.back().resize(byte_size_estimate);
	}

	auto cursor = 0zu;
	auto in_packet = ::AVPacket{};
	auto out_frame = ::AVFrame{};
	auto flushing = false;
	while (!flushing) {
		auto const ret = av_read_frame(format.get(), &in_packet);
		if (ret == AVERROR_EOF)
			flushing = true;
		else
			ret_check(ret);
		ret_check(avcodec_send_packet(codec_ctx.get(), flushing? nullptr : &in_packet));
		av_packet_unref(&in_packet);

		while (true) {
			auto const ret = avcodec_receive_frame(codec_ctx.get(), &out_frame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
			ret_check(ret);

			auto const frame_bytes = out_frame.nb_samples * bytes_per_sample * samples_per_frame;
			if (cursor + frame_bytes > byte_size_estimate) {
				WARN("Underestimated output size by {} bytes", cursor + frame_bytes - byte_size_estimate);
				byte_size_estimate = cursor + frame_bytes;
				for (auto& vec: result->data)
					vec.resize(byte_size_estimate);
			}

			for (auto i: views::iota(0u, planes)) {
				ASSUME(out_frame.data[i]);
				copy(span{reinterpret_cast<byte*>(out_frame.data[i]), frame_bytes},
					result->data[i].begin() + cursor);
			}
			cursor += frame_bytes;
		}
	}

	ASSERT(cursor <= byte_size_estimate);
	for (auto& vec: result->data)
		vec.resize(cursor);
	ASSERT(cursor % bytes_per_sample == 0);
	result->sample_count = cursor / (bytes_per_sample * samples_per_frame);

	return result;
}

auto resample_buffer(DecoderOutput&& input, int sampling_rate) -> vector<Sample>
{
	set_log_callback();
	auto* swr = static_cast<SwrContext*>(nullptr);
	constexpr auto channel_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
	ret_check(swr_alloc_set_opts2(&swr,
		&channel_layout, AV_SAMPLE_FMT_FLT, sampling_rate, &input->channel_layout,
		input->sample_format, static_cast<int>(input->sample_rate), 0, nullptr));
	ret_check(av_opt_set_int(swr, "resampler", SWR_ENGINE_SOXR, 0));
	ret_check(swr_init(swr));

	auto max_out_samples = ret_check(swr_get_out_samples(swr, static_cast<int>(input->sample_count)));
	auto output = vector<Sample>{};
	output.resize(max_out_samples);
	auto in_ptrs = vector<uint8_t const*>{};
	in_ptrs.reserve(input->data.size());
	for (auto const& vec: input->data)
		in_ptrs.push_back(reinterpret_cast<uint8_t const*>(vec.data()));
	auto* out_ptr = reinterpret_cast<uint8_t*>(output.data());
	auto out_samples = ret_check(swr_convert(swr, &out_ptr, output.size(), in_ptrs.data(), input->sample_count));
	out_samples += ret_check(swr_convert(swr, &out_ptr, output.size(), nullptr, 0)); // Flush final samples
	ASSERT(max_out_samples >= out_samples);
	output.resize(out_samples);

	swr_free(&swr);
	delete input;
	return output;
}

auto decode_and_resample_file_buffer(span<byte const> file_contents, int sampling_rate) -> vector<Sample>
{
	set_log_callback();
	auto decoder_output = decode_file_buffer(file_contents);
	auto result = resample_buffer(move(decoder_output), sampling_rate);
	return result;
}

auto encode_as_ogg(span<Sample const> samples, int sampling_rate) -> vector<byte>
{
	set_log_callback();
	auto output = vector<byte>{};
	auto* format_ctx_ptr = static_cast<AVFormatContext*>(nullptr);
	ret_check(avformat_alloc_output_context2(&format_ctx_ptr, nullptr, "ogg", nullptr));
	auto format_ctx = AVFormat{format_ctx_ptr};
	auto* codec = ptr_check(avcodec_find_encoder(AV_CODEC_ID_VORBIS));
	auto codec_ctx = AVCodec{ptr_check(avcodec_alloc_context3(codec))};
	codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
	codec_ctx->sample_rate = sampling_rate;
	codec_ctx->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
	codec_ctx->global_quality = 5 * FF_QP2LAMBDA;
	ret_check(avcodec_open2(codec_ctx.get(), codec, nullptr));

	auto in_frame = AVFrame{ptr_check(av_frame_alloc())};
	auto out_packet = AVPacket{ptr_check(av_packet_alloc())};
	in_frame->nb_samples = codec_ctx->frame_size;
	in_frame->format = AV_SAMPLE_FMT_FLTP;
	in_frame->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
	ret_check(av_frame_get_buffer(in_frame.get(), 0));

	auto* stream = ptr_check(avformat_new_stream(format_ctx.get(), nullptr));
	ret_check(avcodec_parameters_from_context(stream->codecpar, codec_ctx.get()));
	stream->time_base = codec_ctx->time_base;
	auto io_buffer = AVBuffer{av_malloc(PageSize)};
	auto io_ctx = AVIO{ptr_check(avio_alloc_context(
		static_cast<unsigned char*>(io_buffer.get()), PageSize, 1,
		&output, nullptr, &av_io_write, nullptr
	))};
	io_buffer.release();
	format_ctx->pb = io_ctx.get();

	ret_check(avformat_write_header(format_ctx.get(), nullptr));

	auto const encode_and_write = [&](::AVFrame* frame) {
		ret_check(avcodec_send_frame(codec_ctx.get(), frame));
		while (true) {
			auto const ret = avcodec_receive_packet(codec_ctx.get(), out_packet.get());
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
			ret_check(ret);

			out_packet->stream_index = stream->index;
			av_packet_rescale_ts(out_packet.get(), codec_ctx->time_base, stream->time_base);
			ret_check(av_interleaved_write_frame(format_ctx.get(), out_packet.get()));
			av_packet_unref(out_packet.get());
		}
	};

	auto pts = 0z;
	for (auto in_slice: samples | views::chunk(in_frame->nb_samples)) {
		ret_check(av_frame_make_writable(in_frame.get()));
		auto* out_left = reinterpret_cast<float*>(in_frame->data[0]);
		auto* out_right = reinterpret_cast<float*>(in_frame->data[1]);
		ASSUME(out_left);
		ASSUME(out_right);
		in_frame->nb_samples = in_slice.size();
		in_frame->pts = pts;
		pts += in_slice.size();
		transform(in_slice, out_left, [](auto const& s) { return s.left; });
		transform(in_slice, out_right, [](auto const& s) { return s.right; });
		encode_and_write(in_frame.get());
	}

	encode_and_write(nullptr);
	ret_check(av_write_trailer(format_ctx.get()));
	avio_flush(format_ctx->pb);
	return output;
}

auto encode_as_opus(span<Sample const> samples, int sampling_rate) -> vector<byte>
{
	set_log_callback();
	auto output = vector<byte>{};
	auto* format_ctx_ptr = static_cast<AVFormatContext*>(nullptr);
	ret_check(avformat_alloc_output_context2(&format_ctx_ptr, nullptr, "opus", nullptr));
	auto format_ctx = AVFormat{format_ctx_ptr};
	auto* codec = ptr_check(avcodec_find_encoder(AV_CODEC_ID_OPUS));
	auto codec_ctx = AVCodec{ptr_check(avcodec_alloc_context3(codec))};
	codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLT;
	codec_ctx->sample_rate = sampling_rate;
	codec_ctx->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
	codec_ctx->bit_rate = 64 * 1024;
	ret_check(avcodec_open2(codec_ctx.get(), codec, nullptr));

	auto in_frame = AVFrame{ptr_check(av_frame_alloc())};
	auto out_packet = AVPacket{ptr_check(av_packet_alloc())};
	in_frame->nb_samples = codec_ctx->frame_size;
	in_frame->format = AV_SAMPLE_FMT_FLT;
	in_frame->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
	ret_check(av_frame_get_buffer(in_frame.get(), 0));

	auto* stream = ptr_check(avformat_new_stream(format_ctx.get(), nullptr));
	ret_check(avcodec_parameters_from_context(stream->codecpar, codec_ctx.get()));
	stream->time_base = codec_ctx->time_base;
	auto io_buffer = AVBuffer{av_malloc(PageSize)};
	auto io_ctx = AVIO{ptr_check(avio_alloc_context(
		static_cast<unsigned char*>(io_buffer.get()), PageSize, 1,
		&output, nullptr, &av_io_write, nullptr
	))};
	io_buffer.release();
	format_ctx->pb = io_ctx.get();

	ret_check(avformat_write_header(format_ctx.get(), nullptr));

	auto const encode_and_write = [&](::AVFrame* frame) {
		ret_check(avcodec_send_frame(codec_ctx.get(), frame));
		while (true) {
			auto const ret = avcodec_receive_packet(codec_ctx.get(), out_packet.get());
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
			ret_check(ret);

			out_packet->stream_index = stream->index;
			av_packet_rescale_ts(out_packet.get(), codec_ctx->time_base, stream->time_base);
			ret_check(av_interleaved_write_frame(format_ctx.get(), out_packet.get()));
			av_packet_unref(out_packet.get());
		}
	};

	auto pts = 0z;
	for (auto in_slice: samples | views::chunk(in_frame->nb_samples)) {
		ret_check(av_frame_make_writable(in_frame.get()));
		auto* out_buf = reinterpret_cast<Sample*>(in_frame->data[0]);
		ASSUME(out_buf);
		in_frame->nb_samples = in_slice.size();
		in_frame->pts = pts;
		pts += in_slice.size();
		copy(in_slice, out_buf);
		encode_and_write(in_frame.get());
	}

	encode_and_write(nullptr);
	ret_check(av_write_trailer(format_ctx.get()));
	avio_flush(format_ctx->pb);
	return output;
}

}
