use egui::{Context, ViewportId};
use egui_wgpu::{Renderer, ScreenDescriptor};
use egui_winit::State;
use wgpu::{
	CommandEncoder, Device, Extent3d, LoadOp, Operations, Queue, RenderPassColorAttachment,
	RenderPassDescriptor, StoreOp, TextureFormat, TextureView,
};
use winit::window::Window;

pub struct EguiRenderer {
	state: State,
	renderer: Renderer,
}

impl EguiRenderer {
	pub fn new(window: &Window, device: &Device, output_format: TextureFormat) -> EguiRenderer {
		let context = Context::default();
		let state = State::new(
			context,
			ViewportId::ROOT,
			&window,
			Some(window.scale_factor() as f32),
			None,
			Some(2048),
		);
		let renderer = Renderer::new(device, output_format, None, 1, true);
		EguiRenderer { state, renderer }
	}

	pub fn frame<F>(
		&mut self,
		window: &Window,
		device: &Device,
		queue: &Queue,
		encoder: &mut CommandEncoder,
		f: F,
	) where
		F: FnOnce(&Context) -> (TextureView, Extent3d),
	{
		let raw_input = self.state.take_egui_input(window);
		self.state.egui_ctx().begin_pass(raw_input);

		let (output_view, output_size) = f(self.state.egui_ctx());

		let ppi = window.scale_factor() as f32;
		let full_output = self.state.egui_ctx().end_pass();
		self.state
			.handle_platform_output(window, full_output.platform_output);

		let tris = self.state.egui_ctx().tessellate(full_output.shapes, ppi);
		for (id, delta) in &full_output.textures_delta.set {
			self.renderer.update_texture(device, queue, *id, delta);
		}
		let screen_descriptor = ScreenDescriptor {
			size_in_pixels: [output_size.width, output_size.height],
			pixels_per_point: ppi,
		};
		self.renderer
			.update_buffers(device, queue, encoder, &tris, &screen_descriptor);

		let pass = encoder.begin_render_pass(&RenderPassDescriptor {
			label: Some("egui"),
			color_attachments: &[Some(RenderPassColorAttachment {
				view: &output_view,
				resolve_target: None,
				ops: Operations {
					load: LoadOp::Load,
					store: StoreOp::Store,
				},
			})],
			depth_stencil_attachment: None,
			occlusion_query_set: None,
			timestamp_writes: None,
		});
		self.renderer
			.render(&mut pass.forget_lifetime(), &tris, &screen_descriptor);

		for tex in &full_output.textures_delta.free {
			self.renderer.free_texture(tex);
		}
	}
}
