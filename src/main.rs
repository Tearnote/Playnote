use std::fs::File;
use std::sync::Arc;
use anyhow::{Context, Result};
use tracing::info;
use tracing_subscriber::{prelude::*, fmt};
use tracing_subscriber::filter::{EnvFilter, LevelFilter};
use winit::application::ApplicationHandler;
use winit::dpi::PhysicalSize;
use winit::event::WindowEvent;
use winit::event_loop::{ActiveEventLoop, ControlFlow, EventLoop};
use winit::window::{Window, WindowAttributes, WindowId};

#[derive(Default)]
struct App {
	window: Option<Window>,
}

impl ApplicationHandler for App {
	fn resumed(&mut self, event_loop: &ActiveEventLoop) {
		let attributes = WindowAttributes::default()
			.with_inner_size(PhysicalSize::new(1280, 720))
			.with_resizable(false)
			.with_title(env!("CARGO_PKG_NAME"));
		self.window = Some(event_loop.create_window(attributes)
			.context("Failed to create application window")
			.unwrap_or_else(|e| panic!("{}", e.to_string())));
	}

	fn window_event(&mut self, event_loop: &ActiveEventLoop, _window_id: WindowId, event: WindowEvent) {
		match event {
			WindowEvent::CloseRequested => {
				event_loop.exit();
			}
			_ => (),
		}
	}
}

fn main() -> Result<()> {
	init_logging()?;
	const NAME: &str = env!("CARGO_PKG_NAME");
	const VERSION: &str = env!("CARGO_PKG_VERSION");
	info!("Initializing {NAME} {VERSION}");

	let event_loop = EventLoop::new().context("Failed to initialize windowing")?;
	event_loop.set_control_flow(ControlFlow::Poll);
	let mut app = App::default();
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
