use super::{ThreadShared, UserEvent};
use anyhow::Result;
use std::sync::{Arc, Mutex};
use std::sync::atomic::{self, AtomicBool};
use std::thread::{self, JoinHandle};
use tracing::{error, info, instrument};
use winit::application::ApplicationHandler;
use winit::dpi::PhysicalSize;
use winit::event::WindowEvent;
use winit::event_loop::{ActiveEventLoop, ControlFlow, EventLoop, EventLoopProxy};
use winit::window::{WindowAttributes, WindowId};

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
		let App::Initializing(proxy) = self else {
			return;
		};

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
			},
		};
		info!(target: "input", "Created application window with {:?}", window.inner_size());

		let shared = ThreadShared {
			proxy: proxy.clone(),
			running: Arc::new(AtomicBool::new(true)),
			window,
			input_handlers: Arc::new(Mutex::new(vec![])),
		};
		let render_shared = shared.clone();
		let render_thread_handle = thread::spawn(move || super::render(render_shared));

		let ctx = AppContext {
			shared,
			render_thread_handle: Some(render_thread_handle),
		};
		*self = App::Initialized(ctx);
	}

	fn user_event(&mut self, event_loop: &ActiveEventLoop, event: UserEvent) {
		match event {
			UserEvent::Quit => event_loop.exit(),
		}
	}

	fn window_event(&mut self, event_loop: &ActiveEventLoop, _: WindowId, event: WindowEvent) {
		let App::Initialized(ctx) = self else { return };
		if let Ok(mut handlers) = ctx.shared.input_handlers.lock() {
			for handler in handlers.iter_mut() {
				if !handler(event.clone()) { return };
			}
		}
		match event {
			WindowEvent::CloseRequested => {
				info!(target: "input", "Application close requested");
				event_loop.exit();
			},
			_ => (),
		}
	}

	fn exiting(&mut self, _: &ActiveEventLoop) {
		let App::Initialized(ctx) = self else { return };
		ctx.shared.running.store(false, atomic::Ordering::Relaxed);
		ctx.render_thread_handle
			.take()
			.unwrap()
			.join()
			.expect("Render thread panicked");
		*self = App::Uninitialized;
	}
}
