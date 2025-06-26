use std::sync::atomic::Ordering;
use anyhow::{Context as AnyhowContext, Result};
use tracing::{debug, error, info, instrument};
use wgpu::{
	BackendOptions, Backends, Device, DeviceDescriptor, Features, Instance, InstanceDescriptor,
	InstanceFlags, Limits, MemoryHints, PowerPreference, Queue, RequestAdapterOptions, Surface,
	Trace,
};
use winit::dpi::PhysicalSize;
use winit::window::Window;
use super::ThreadShared;

struct Context<'a> {
	surface: Surface<'a>,
	device: Device,
	queue: Queue,
}

pub fn render(shared: ThreadShared) {
	pollster::block_on(render_inner(&shared)).unwrap_or_else(|e| {
		error!(target: "render", "{e}");
		shared.running.store(false, Ordering::Relaxed);
	});
}

async fn render_inner(shared: &ThreadShared) -> Result<()> {
	let context = init_gpu(shared.window.as_ref()).await.context("GPU initialization failed")?;

	while shared.running.load(Ordering::Relaxed) {
		// let output = context.surface.get_current_texture();
	}
	Ok(())
}

#[instrument(target = "render", name = "gpu_init", skip_all)]
async fn init_gpu(window: &Window) -> Result<Context> {
	let instance = Instance::new(&InstanceDescriptor {
		backends: Backends::PRIMARY,
		flags: InstanceFlags::from_env_or_default(),
		backend_options: BackendOptions::from_env_or_default(),
	});
	debug!(target: "render", "Created wgpu instance");

	let surface = instance.create_surface(window)?;
	let adapter = instance.request_adapter(&RequestAdapterOptions {
		power_preference: PowerPreference::LowPower,
		force_fallback_adapter: false,
		compatible_surface: Some(&surface),
	}).await?;
	info!(target: "render", "Using GPU: {}", adapter.get_info().name);

	let (device, queue) = adapter.request_device(&DeviceDescriptor {
		label: Some("my epic gpu"),
		required_features: Features::default(),
		required_limits: Limits::downlevel_defaults(),
		memory_hints: MemoryHints::Performance,
		trace: Trace::Off,
	}).await?;
	Ok(Context {
		surface,
		device,
		queue,
	})
}

fn configure_surface(surface: &Surface, size: PhysicalSize<u32>) {}
