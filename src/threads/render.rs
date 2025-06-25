use crossbeam_channel::Receiver;
use std::sync::Arc;
use wgpu::wgt::DeviceDescriptor;
use wgpu::{BackendOptions, Backends, Features, Instance, InstanceDescriptor, InstanceFlags, Limits, MemoryHints, PowerPreference, RequestAdapterOptions, Trace};
use winit::window::Window;

pub async fn render(window_rx: Receiver<Arc<Window>>) {
	let instance = Instance::new(&InstanceDescriptor {
		backends: Backends::PRIMARY,
		flags: InstanceFlags::from_env_or_default(),
		backend_options: BackendOptions::from_env_or_default(),
	});
	let surface = instance.create_surface(window_rx.recv().expect("Window channel closed"))
		.expect("Failed to create GPU surface");
	let adapter = instance.request_adapter(&RequestAdapterOptions {
		power_preference: PowerPreference::LowPower,
		force_fallback_adapter: false,
		compatible_surface: Some(&surface),
	}).await.expect("No compatible GPU found");
	let (device, queue) = adapter.request_device(&DeviceDescriptor {
		label: Some("my epic gpu"),
		required_features: Features::default(),
		required_limits: Limits::downlevel_defaults(),
		memory_hints: MemoryHints::Performance,
		trace: Trace::Off,
	}).await.expect("Failed to request a compatible GPU");
}
