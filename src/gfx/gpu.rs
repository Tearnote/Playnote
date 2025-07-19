use anyhow::Result;
use tracing::{debug, info, instrument};
use wgpu::wgt::SurfaceConfiguration;
use wgpu::{
	BackendOptions, Backends, CompositeAlphaMode, Device, DeviceDescriptor, Features, Instance,
	InstanceDescriptor, InstanceFlags, Limits, MemoryHints, PowerPreference, PresentMode, Queue,
	RequestAdapterOptions, Surface, TextureFormat, TextureUsages, Trace,
};
use winit::dpi::PhysicalSize;
use winit::window::Window;

pub struct GPU<'a> {
	pub surface: Surface<'a>,
	pub device: Device,
	pub queue: Queue,
}

impl GPU<'_> {
	pub const SURFACE_FORMAT: TextureFormat = TextureFormat::Bgra8Unorm;

	#[instrument(target = "render", skip_all)]
	pub async fn new(window: &Window) -> Result<GPU> {
		let instance = Instance::new(&InstanceDescriptor {
			backends: Backends::PRIMARY,
			flags: InstanceFlags::from_env_or_default(),
			backend_options: BackendOptions::from_env_or_default(),
		});
		debug!(target: "render", "Created wgpu instance");

		let surface = instance.create_surface(window)?;
		let adapter = instance
			.request_adapter(&RequestAdapterOptions {
				power_preference: PowerPreference::LowPower,
				force_fallback_adapter: false,
				compatible_surface: Some(&surface),
			})
			.await?;
		info!(target: "render", "Using GPU: {}", adapter.get_info().name);

		let (device, queue) = adapter
			.request_device(&DeviceDescriptor {
				label: Some("my epic gpu"),
				required_features: Features::default(),
				required_limits: Limits::downlevel_defaults(),
				memory_hints: MemoryHints::Performance,
				trace: Trace::Off,
			})
			.await?;

		let result = GPU {
			surface,
			device,
			queue,
		};
		result.configure_surface(window.inner_size());
		Ok(result)
	}

	pub fn configure_surface(&self, size: PhysicalSize<u32>) {
		self.surface.configure(
			&self.device,
			&SurfaceConfiguration {
				usage: TextureUsages::RENDER_ATTACHMENT,
				format: GPU::SURFACE_FORMAT,
				width: size.width,
				height: size.height,
				present_mode: PresentMode::AutoVsync,
				desired_maximum_frame_latency: 2,
				alpha_mode: CompositeAlphaMode::Auto,
				view_formats: vec![TextureFormat::Bgra8UnormSrgb],
			},
		);
	}
}
