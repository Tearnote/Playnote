use std::fs::File;
use std::sync::Arc;
use anyhow::{Context, Result};
use tracing::info;
use tracing_subscriber::{prelude::*, fmt};
use tracing_subscriber::filter::{EnvFilter, LevelFilter};

fn main() -> Result<()> {
	init_logging()?;
	const NAME: &str = env!("CARGO_PKG_NAME");
	const VERSION: &str = env!("CARGO_PKG_VERSION");
	info!("Initializing {NAME} {VERSION}");
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
