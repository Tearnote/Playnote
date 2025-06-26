use std::sync::Arc;
use std::sync::atomic::{self, AtomicBool};
use std::thread;
use tracing::{info, instrument};
use winit::application::ApplicationHandler;
use winit::dpi::PhysicalSize;
use winit::event::WindowEvent;
use winit::event_loop::{ActiveEventLoop, ControlFlow, EventLoop};
use winit::window::{WindowAttributes, WindowId};
use super::ThreadShared;

pub fn input() {
	let event_loop = EventLoop::new().expect("Failed to initialize OS windowing");
	event_loop.set_control_flow(ControlFlow::Poll);
	let mut app = App::Uninitialized;
	event_loop.run_app(&mut app).expect("OS error");
}

enum App {
	Uninitialized,
	Initialized(AppContext),
}

struct AppContext {
	shared: ThreadShared,
	render_thread_handle: Option<thread::JoinHandle<()>>,
}

impl ApplicationHandler for App {
	#[instrument(target = "input", name = "window_init", skip_all)]
	fn resumed(&mut self, event_loop: &ActiveEventLoop) {
		if matches!(self, App::Initialized(_)) { return }

		let attributes = WindowAttributes::default()
			.with_inner_size(PhysicalSize::new(1280, 720))
			.with_resizable(false)
			.with_title(env!("CARGO_PKG_NAME"));
		let window = Arc::new(event_loop.create_window(attributes)
			.expect("Failed to create application window"));
		info!(target: "input", "Created application window with {:?}", window.inner_size());

		let shared = ThreadShared {
			running: Arc::new(AtomicBool::new(true)),
			window,
		};
		let render_shared = shared.clone();
		let render_thread_handle = thread::spawn(move || {
			pollster::block_on(super::render(render_shared))
		});

		let ctx = AppContext {
			shared,
			render_thread_handle: Some(render_thread_handle),
		};
		*self = App::Initialized(ctx);
	}

	fn window_event(&mut self, event_loop: &ActiveEventLoop, _: WindowId, event: WindowEvent) {
		match event {
			WindowEvent::CloseRequested => {
				info!(target: "input", "Application close requested");
				event_loop.exit();
			}
			_ => (),
		}
	}

	fn exiting(&mut self, _: &ActiveEventLoop) {
		let App::Initialized(ctx) = self else { return };
		ctx.shared.running.store(false, atomic::Ordering::Relaxed);
		let render_thread_handle = ctx.render_thread_handle.take().unwrap();
		render_thread_handle.join().expect("Render thread panicked");
		*self = App::Uninitialized;
	}
}
