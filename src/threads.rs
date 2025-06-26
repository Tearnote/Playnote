mod render;
mod input;

use std::sync::Arc;
use std::sync::atomic::AtomicBool;
use winit::event_loop::EventLoopProxy;
pub use self::render::render;
pub use self::input::input;

#[derive(Clone)]
pub struct ThreadShared {
	pub proxy: EventLoopProxy<UserEvent>,
	pub running: Arc<AtomicBool>,
	pub window: Arc<winit::window::Window>,
}

pub enum UserEvent {
	Quit,
}
