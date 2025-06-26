mod threads;

use std::fs::File;
use std::sync::Arc;
use std::thread;
use std::panic::panic_any;
use std::sync::atomic::{AtomicBool, Ordering};
use std::thread::JoinHandle;
use anyhow::{Context, Result};
use tracing::{info, instrument};
use tracing_subscriber::{prelude::*, fmt};
use tracing_subscriber::filter::{EnvFilter, LevelFilter};
use winit::application::ApplicationHandler;
use winit::dpi::PhysicalSize;
use winit::event::WindowEvent;
use winit::event_loop::{ActiveEventLoop, ControlFlow, EventLoop};
use winit::window::{WindowAttributes, WindowId};
use crate::threads::ThreadShared;

enum App {
	Uninitialized,
	Initialized(AppContext),
}

struct AppContext {
	shared: ThreadShared,
	render_thread_handle: Option<JoinHandle<()>>,
}

impl ApplicationHandler for App {
	#[instrument(target = "main", name = "window_init", skip_all)]
	fn resumed(&mut self, event_loop: &ActiveEventLoop) {
		if matches!(self, App::Initialized(_)) { return }

		let attributes = WindowAttributes::default()
			.with_inner_size(PhysicalSize::new(1280, 720))
			.with_resizable(false)
			.with_title(env!("CARGO_PKG_NAME"));
		let window = Arc::new(event_loop.create_window(attributes)
			.expect("Failed to create application window"));
		info!(target: "main", "Created application window with {:?}", window.inner_size());

		let shared = ThreadShared {
			running: Arc::new(AtomicBool::new(true)),
			window,
		};
		let render_shared = shared.clone();
		let render_thread_handle = thread::spawn(move || {
			pollster::block_on(threads::render(render_shared))
		});

		let ctx = AppContext {
			shared,
			render_thread_handle: Some(render_thread_handle),
		};
		*self = App::Initialized(ctx);
	}

	fn window_event(&mut self, event_loop: &ActiveEventLoop, _window_id: WindowId, event: WindowEvent) {
		match event {
			WindowEvent::CloseRequested => {
				info!(target: "main", "Application close requested");
				event_loop.exit();
			}
			_ => (),
		}
	}

	fn exiting(&mut self, _event_loop: &ActiveEventLoop) {
		let App::Initialized(ctx) = self else { return };
		ctx.shared.running.store(false, Ordering::Relaxed);
		let render_thread_handle = ctx.render_thread_handle.take().unwrap();
		render_thread_handle.join().unwrap_or_else(|e| panic_any(e));
		*self = App::Uninitialized;
	}
}

fn main() -> Result<()> {
	init_logging()?;
	const NAME: &str = env!("CARGO_PKG_NAME");
	const VERSION: &str = env!("CARGO_PKG_VERSION");
	info!(target: "main", "Starting up {NAME} {VERSION}");

	let event_loop = EventLoop::new().context("Failed to initialize windowing")?;
	event_loop.set_control_flow(ControlFlow::Poll);
	let mut app = App::Uninitialized;
	event_loop.run_app(&mut app)?;

	Ok(())
}

fn init_logging() -> Result<()> {
	const DEFAULT_LEVEL: LevelFilter = if cfg!(debug_assertions) { LevelFilter::DEBUG } else { LevelFilter::INFO };
	const FILENAME: &str = if cfg!(debug_assertions) { "playnote-debug.log" } else { "playnote.log" };
	let file = File::create(FILENAME)
		.with_context(|| format!("Failed to open log file \"{}\" for writing", FILENAME))?;

	let filter = EnvFilter::builder()
		.with_default_directive(DEFAULT_LEVEL.into())
		.with_env_var("PLAYNOTE_LOG")
		.from_env_lossy();
	let con_writer = fmt::layer();
	let file_writer = fmt::layer()
		.with_writer(Arc::new(file))
		.with_ansi(false);
	tracing_subscriber::registry()
		.with(filter)
		.with(con_writer)
		.with(file_writer)
		.init();
	Ok(())
}
