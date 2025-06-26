mod render;
mod input;

use std::sync::Arc;
use std::sync::atomic::AtomicBool;

pub use self::render::render;
pub use self::input::input;

#[derive(Clone)]
pub struct ThreadShared {
	pub running: Arc<AtomicBool>,
	pub window: Arc<winit::window::Window>,
}
