use std::sync::atomic::Ordering;
use tracing::{debug, info, instrument};
use crate::threads::ThreadShared;
use wgpu::wgt::DeviceDescriptor;
use wgpu::{BackendOptions, Backends, Features, Instance, InstanceDescriptor, InstanceFlags, Limits, MemoryHints, PowerPreference, RequestAdapterOptions, Trace};
use winit::window::Window;

struct Context {
	device: wgpu::Device,
	queue: wgpu::Queue,
}

pub async fn render(shared: ThreadShared) {
	let _context = init_gpu(shared.window.as_ref()).await;

	while shared.running.load(Ordering::Relaxed) {

	}
}

#[instrument(target = "render", name = "gpu_init", skip_all)]
async fn init_gpu(window: &Window) -> Context {
	let instance = Instance::new(&InstanceDescriptor {
		backends: Backends::PRIMARY,
		flags: InstanceFlags::from_env_or_default(),
		backend_options: BackendOptions::from_env_or_default(),
	});
	debug!(target: "render", "Created wgpu instance");

	let surface = instance.create_surface(window)
		.expect("Failed to create GPU surface");
	let adapter = instance.request_adapter(&RequestAdapterOptions {
		power_preference: PowerPreference::LowPower,
		force_fallback_adapter: false,
		compatible_surface: Some(&surface),
	}).await.expect("No compatible GPU found");
	info!(target: "render", "Using GPU: {}", adapter.get_info().name);

	let (device, queue) = adapter.request_device(&DeviceDescriptor {
		label: Some("my epic gpu"),
		required_features: Features::default(),
		required_limits: Limits::downlevel_defaults(),
		memory_hints: MemoryHints::Performance,
		trace: Trace::Off,
	}).await.expect("Failed to request a compatible GPU");
	Context {
		device,
		queue,
	}
}
