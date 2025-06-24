use std::fs::File;
use std::sync::Arc;
use anyhow::{Context, Result};
use tracing::info;
use tracing_subscriber::{prelude::*, fmt};
use tracing_subscriber::filter::{EnvFilter, LevelFilter};

fn main() -> Result<()> {
	init_logging()?;
	info!("Initializing Playnote 0.0.0");
	Ok(())
}

fn init_logging() -> Result<()> {
	let default_level = if cfg!(debug_assertions) { LevelFilter::DEBUG } else { LevelFilter::INFO };
	let filename = if cfg!(debug_assertions) { "playnote-debug.log" } else { "playnote.log" };
	let file = File::create(filename)
		.with_context(|| format!("Failed to open log file \"{}\" for writing", filename))?;
	
	let filter = EnvFilter::builder()
		.with_default_directive(default_level.into())
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
