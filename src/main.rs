mod gfx;
mod threads;

use anyhow::{Context, Result};
use std::fs;
use std::sync::Arc;
use tracing::info;
use tracing_subscriber::filter::{EnvFilter, LevelFilter};
use tracing_subscriber::{fmt, prelude::*};

fn main() -> Result<()> {
	init_logging()?;
	const NAME: &str = env!("CARGO_PKG_NAME");
	const VERSION: &str = env!("CARGO_PKG_VERSION");
	info!(target: "input", "Starting up {NAME} {VERSION}");
	threads::input()?;
	Ok(())
}

fn init_logging() -> Result<()> {
	const DEFAULT_LEVEL: LevelFilter = if cfg!(debug_assertions) {
		LevelFilter::DEBUG
	} else {
		LevelFilter::INFO
	};
	const FILENAME: &str = if cfg!(debug_assertions) {
		"playnote-debug.log"
	} else {
		"playnote.log"
	};
	let file = fs::File::create(FILENAME)
		.with_context(|| format!("Failed to open log file \"{}\" for writing", FILENAME))?;

	let filter = EnvFilter::builder()
		.with_default_directive(DEFAULT_LEVEL.into())
		.with_env_var("PLAYNOTE_LOG")
		.from_env_lossy();
	let con_writer = fmt::layer();
	let file_writer = fmt::layer().with_writer(Arc::new(file)).with_ansi(false);
	tracing_subscriber::registry()
		.with(filter)
		.with(con_writer)
		.with(file_writer)
		.init();
	Ok(())
}
