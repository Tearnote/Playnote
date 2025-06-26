mod render;

use std::sync::Arc;
use std::sync::atomic::AtomicBool;
use winit::window::Window;
pub use render::render;

#[derive(Clone)]
pub struct ThreadShared {
	pub running: Arc<AtomicBool>,
	pub window: Arc<Window>,
}
