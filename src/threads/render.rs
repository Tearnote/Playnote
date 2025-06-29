use std::sync::atomic::Ordering;
use anyhow::{Context as AnyhowContext, Result};
use tracing::{debug, error, info, instrument};
use wgpu::{BackendOptions, Backends, Color, CompositeAlphaMode, Device, DeviceDescriptor, Features, Instance, InstanceDescriptor, InstanceFlags, Limits, LoadOp, MemoryHints, Operations, PowerPreference, PresentMode, Queue, RenderPassColorAttachment, RenderPassDescriptor, RequestAdapterOptions, StoreOp, Surface, SurfaceError, TextureFormat, TextureUsages, TextureViewDescriptor, Trace};
use wgpu::wgt::{CommandEncoderDescriptor, SurfaceConfiguration};
use winit::dpi::PhysicalSize;
use winit::window::Window;
use super::{ThreadShared, UserEvent};

struct Context<'a> {
	surface: Surface<'a>,
	device: Device,
	queue: Queue,
}

pub fn render(shared: ThreadShared) {
	pollster::block_on(render_inner(&shared)).unwrap_or_else(|e| {
		error!(target: "render", "{e}");
		let _ = shared.proxy.send_event(UserEvent::Quit);
	});
}

async fn render_inner(shared: &ThreadShared) -> Result<()> {
	let context = init_gpu(shared.window.as_ref()).await.context("GPU initialization failed")?;

	while shared.running.load(Ordering::Relaxed) {
		let output = match context.surface.get_current_texture() {
			Ok(output) => output,
			Err(SurfaceError::Lost | SurfaceError::Outdated) => {
				configure_surface(&context.surface, &context.device, shared.window.inner_size());
				continue;
			}
			Err(e) => return Err(e.into())
		};

		let mut encoder = context.device.create_command_encoder(&CommandEncoderDescriptor::default());
		let surface_view = output.texture.create_view(&TextureViewDescriptor::default());

		drop(encoder.begin_render_pass(&RenderPassDescriptor {
			label: Some("clear"),
			color_attachments: &[Some(RenderPassColorAttachment {
				view: &surface_view,
				resolve_target: None,
				ops: Operations {
					load: LoadOp::Clear(Color {r: 0.060, g: 0.060, b: 0.060, a: 1.000}),
					store: StoreOp::Store,
				},
			})],
			depth_stencil_attachment: None,
			occlusion_query_set: None,
			timestamp_writes: None,
		}));

		context.queue.submit(Some(encoder.finish()));
		output.present();
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
	configure_surface(&surface, &device, window.inner_size());
	Ok(Context {
		surface,
		device,
		queue,
	})
}

fn configure_surface(surface: &Surface, device: &Device, size: PhysicalSize<u32>) {
	surface.configure(device, &SurfaceConfiguration {
		usage: TextureUsages::RENDER_ATTACHMENT,
		format: TextureFormat::Bgra8UnormSrgb,
		width: size.width,
		height: size.height,
		present_mode: PresentMode::AutoVsync,
		desired_maximum_frame_latency: 2,
		alpha_mode: CompositeAlphaMode::Auto,
		view_formats: vec![TextureFormat::Bgra8UnormSrgb],
	});
}
