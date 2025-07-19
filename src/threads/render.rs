use super::{ThreadShared, UserEvent};
use crate::gfx::{EguiRenderer, GPU};
use anyhow::{Context as AnyhowContext, Result};
use std::sync::atomic::Ordering;
use std::sync::mpsc;
use tracing::error;
use wgpu::wgt::CommandEncoderDescriptor;
use wgpu::{
	Color, LoadOp, Operations, RenderPassColorAttachment, RenderPassDescriptor, StoreOp,
	SurfaceError, TextureViewDescriptor,
};

pub fn render(shared: ThreadShared) {
	pollster::block_on(render_inner(&shared)).unwrap_or_else(|e| {
		error!(target: "render", "{e}");
		let _ = shared.proxy.send_event(UserEvent::Quit);
	});
}

async fn render_inner(shared: &ThreadShared) -> Result<()> {
	let gpu = GPU::new(shared.window.as_ref())
		.await
		.context("GPU initialization failed")?;
	let mut egui_renderer = EguiRenderer::new(&shared.window, &gpu, GPU::SURFACE_FORMAT);

	let (window_event_tx, window_event_rx) = mpsc::channel();
	if let Ok(mut handlers) = shared.input_handlers.lock() {
		handlers.push(Box::new(move |event| {
			let _ = window_event_tx.send(event);
			true
		}));
	}

	while shared.running.load(Ordering::Relaxed) {
		for event in window_event_rx.try_iter() {
			egui_renderer.handle_input(&shared.window, &event);
		}

		let output = match gpu.surface.get_current_texture() {
			Ok(output) => output,
			Err(SurfaceError::Lost | SurfaceError::Outdated) => {
				gpu.configure_surface(shared.window.inner_size());
				continue;
			},
			Err(e) => return Err(e.into()),
		};

		let mut encoder = gpu
			.device
			.create_command_encoder(&CommandEncoderDescriptor::default());
		let surface_view = output
			.texture
			.create_view(&TextureViewDescriptor::default());

		drop(encoder.begin_render_pass(&RenderPassDescriptor {
			label: Some("clear"),
			color_attachments: &[Some(RenderPassColorAttachment {
				view: &surface_view,
				resolve_target: None,
				ops: Operations {
					load: LoadOp::Clear(Color {
						r: 0.060,
						g: 0.060,
						b: 0.060,
						a: 1.000,
					}),
					store: StoreOp::Store,
				},
			})],
			depth_stencil_attachment: None,
			occlusion_query_set: None,
			timestamp_writes: None,
		}));

		egui_renderer.frame(
			shared.window.as_ref(),
			&gpu.device,
			&gpu.queue,
			&mut encoder,
			|ctx| {
				egui::Window::new("Hello World").show(&ctx, |ui| {
					ui.label("Hello World!");
				});
				(surface_view, output.texture.size())
			},
		);

		gpu.queue.submit(Some(encoder.finish()));
		output.present();
	}
	Ok(())
}
