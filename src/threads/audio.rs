use super::{ThreadShared, UserEvent};
use anyhow::Result;
use pipewire::context::Context;
use pipewire::properties::properties;
use pipewire::spa::param::ParamType;
use pipewire::spa::param::audio::{AudioFormat, AudioInfoRaw};
use pipewire::spa::pod::serialize::PodSerializer;
use pipewire::spa::pod::{Object, Pod, Property, Value};
use pipewire::spa::utils::{Direction, SpaTypes};
use pipewire::stream::{Stream, StreamFlags};
use pipewire::thread_loop::ThreadLoop;
use std::io::Cursor;
use std::sync::atomic::Ordering;
use std::thread;
use tracing::error;

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
	while shared.running.load(Ordering::Relaxed) {
		thread::yield_now();
	}
	thread_loop.stop();
	Ok(())
}
