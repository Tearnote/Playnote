use std::sync::Arc;
use std::sync::atomic::{self, AtomicBool};
use std::thread::{self, JoinHandle};
use anyhow::Result;
use tracing::{error, info, instrument};
use winit::application::ApplicationHandler;
use winit::dpi::PhysicalSize;
use winit::event::WindowEvent;
use winit::event_loop::{ActiveEventLoop, ControlFlow, EventLoop, EventLoopProxy};
use winit::window::{WindowAttributes, WindowId};
use super::{ThreadShared, UserEvent};

pub fn input() -> Result<()> {
	let event_loop = EventLoop::<UserEvent>::with_user_event().build()?;
	event_loop.set_control_flow(ControlFlow::Poll);
	let mut app = App::Initializing(event_loop.create_proxy());
	event_loop.run_app(&mut app)?;
	Ok(())
}

enum App {
	Uninitialized,
	Initializing(EventLoopProxy<UserEvent>),
	Initialized(AppContext),
}

struct AppContext {
	shared: ThreadShared,
	render_thread_handle: Option<JoinHandle<()>>,
}

impl ApplicationHandler<UserEvent> for App {
	#[instrument(target = "input", name = "window_init", skip_all)]
	fn resumed(&mut self, event_loop: &ActiveEventLoop) {
		let App::Initializing(proxy) = self else { return };

		let attributes = WindowAttributes::default()
			.with_inner_size(PhysicalSize::new(1280, 720))
			.with_resizable(false)
			.with_title(env!("CARGO_PKG_NAME"));
		let window = match event_loop.create_window(attributes) {
			Ok(w) => Arc::new(w),
			Err(e) => {
				error!(target: "input", "Failed to create application window: {e}");
				event_loop.exit();
				return;
			}
		};
		info!(target: "input", "Created application window with {:?}", window.inner_size());

		let shared = ThreadShared {
			proxy: proxy.clone(),
			running: Arc::new(AtomicBool::new(true)),
			window,
		};
		let render_shared = shared.clone();
		let render_thread_handle = thread::spawn(move || { super::render(render_shared) });

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
	
	fn user_event(&mut self, event_loop: &ActiveEventLoop, event: UserEvent) {
		match event {
			UserEvent::Quit => event_loop.exit(),
		}
	}

	fn exiting(&mut self, _: &ActiveEventLoop) {
		let App::Initialized(ctx) = self else { return };
		ctx.shared.running.store(false, atomic::Ordering::Relaxed);
		ctx.render_thread_handle.take().unwrap().join().expect("Render thread panicked");
		*self = App::Uninitialized;
	}
}
