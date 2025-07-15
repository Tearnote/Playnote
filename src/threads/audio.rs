use super::{ThreadShared, UserEvent};
use anyhow::Result;
use pipewire::context::Context;
use pipewire::properties::properties;
use pipewire::spa::param::ParamType;
use pipewire::spa::param::audio::{AudioFormat, AudioInfoRaw};
use pipewire::spa::param::format::{MediaSubtype, MediaType};
use pipewire::spa::param::format_utils::parse_format;
use pipewire::spa::pod::serialize::PodSerializer;
use pipewire::spa::pod::{Object, Pod, Property, Value};
use pipewire::spa::utils::{Direction, SpaTypes};
use pipewire::stream::{Stream, StreamFlags};
use pipewire::thread_loop::ThreadLoop;
use std::io::Cursor;
use std::sync::atomic::Ordering;
use std::sync::{Arc, Mutex};
use std::thread;
use tracing::{error, info};

const AUDIO_LATENCY: u32 = 128;

pub fn audio(shared: ThreadShared) {
	audio_inner(&shared).unwrap_or_else(|e| {
		error!(target: "audio", "{e}");
		let _ = shared.proxy.send_event(UserEvent::Quit);
	})
}

fn audio_inner(shared: &ThreadShared) -> Result<()> {
	let thread_loop = unsafe { ThreadLoop::new(Some("audio"), None) }?;
	let context = Context::new(&thread_loop)?;
	let core = context.connect(None)?;
	let stream = Stream::new(
		&core,
		env!("CARGO_PKG_NAME"),
		properties! {
			"media.type"         => "Audio",
			"media.category"     => "Playback",
			"media.role"         => "Game",
			"node.force-quantum" => format!("{AUDIO_LATENCY}"),
		},
	)?;

	let sample_rate: Arc<Mutex<Option<u32>>> = Arc::new(Mutex::new(None));
	let sample_rate_copy = sample_rate.clone();
	let _stream_listener = stream
		.add_local_listener::<()>()
		.param_changed(move |_, _, id, param| {
			let Some(param) = param else { return };
			if id != ParamType::Format.as_raw() {
				return;
			};
			let Ok((media_type, media_subtype)) = parse_format(param) else {
				return;
			};
			if media_type != MediaType::Audio || media_subtype != MediaSubtype::Raw {
				return;
			};
			let mut audio_info = AudioInfoRaw::default();
			let Ok(_) = audio_info.parse(param) else {
				return;
			};
			if audio_info.rate() <= 0 {
				return;
			};
			let Ok(mut sample_rate) = sample_rate_copy.lock() else {
				return;
			};
			*sample_rate = Some(audio_info.rate());
			info!(target: "audio", "Sample rate negotiated to {}", audio_info.rate());
		})
		.register()?;

	let mut audio_info = AudioInfoRaw::new();
	audio_info.set_format(AudioFormat::F32LE);
	audio_info.set_channels(2);
	let audio_info = Object {
		type_: SpaTypes::ObjectParamFormat.as_raw(),
		id: ParamType::EnumFormat.as_raw(),
		properties: Vec::<Property>::from(audio_info),
	};
	let audio_info = PodSerializer::serialize(Cursor::new(vec![]), &Value::Object(audio_info))
		.expect("SPA pod serialization failed")
		.0
		.into_inner();
	let mut params = [Pod::from_bytes(&audio_info).expect("Invalid SPA pod data")];

	stream.connect(
		Direction::Output,
		None,
		StreamFlags::AUTOCONNECT | StreamFlags::MAP_BUFFERS | StreamFlags::RT_PROCESS,
		&mut params,
	)?;

	thread_loop.start();
	info!(target: "audio", "pipewire audio initialized");
	while shared.running.load(Ordering::Relaxed) {
		thread::yield_now();
	}
	thread_loop.stop();
	Ok(())
}
