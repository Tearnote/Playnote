use std::sync::atomic;
use tracing::{debug, info, instrument};
use super::ThreadShared;

struct Context<'a> {
	surface: wgpu::Surface<'a>,
	device: wgpu::Device,
	queue: wgpu::Queue,
}

pub async fn render(shared: ThreadShared) {
	let context = init_gpu(shared.window.as_ref()).await;

	while shared.running.load(atomic::Ordering::Relaxed) {
		// let output = context.surface.get_current_texture();
	}
}

#[instrument(target = "render", name = "gpu_init", skip_all)]
async fn init_gpu(window: &winit::window::Window) -> Context {
	let instance = wgpu::Instance::new(&wgpu::InstanceDescriptor {
		backends: wgpu::Backends::PRIMARY,
		flags: wgpu::InstanceFlags::from_env_or_default(),
		backend_options: wgpu::BackendOptions::from_env_or_default(),
	});
	debug!(target: "render", "Created wgpu instance");

	let surface = instance.create_surface(window)
		.expect("Failed to create GPU surface");
	let adapter = instance.request_adapter(&wgpu::RequestAdapterOptions {
		power_preference: wgpu::PowerPreference::LowPower,
		force_fallback_adapter: false,
		compatible_surface: Some(&surface),
	}).await.expect("No compatible GPU found");
	info!(target: "render", "Using GPU: {}", adapter.get_info().name);

	let (device, queue) = adapter.request_device(&wgpu::DeviceDescriptor {
		label: Some("my epic gpu"),
		required_features: wgpu::Features::default(),
		required_limits: wgpu::Limits::downlevel_defaults(),
		memory_hints: wgpu::MemoryHints::Performance,
		trace: wgpu::Trace::Off,
	}).await.expect("Failed to request a compatible GPU");
	Context { surface, device, queue }
}

fn configure_surface(surface: &wgpu::Surface, size: winit::dpi::PhysicalSize<u32>) {
	
}
