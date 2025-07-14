mod audio;
mod input;
mod render;

pub use self::audio::audio;
pub use self::input::input;
pub use self::render::render;
use std::sync::atomic::AtomicBool;
use std::sync::{Arc, Mutex};
use winit::event::WindowEvent;
use winit::event_loop::EventLoopProxy;

#[derive(Clone)]
pub struct ThreadShared {
	pub proxy: EventLoopProxy<UserEvent>,
	pub running: Arc<AtomicBool>,
	pub window: Arc<winit::window::Window>,
	pub input_handlers: Arc<Mutex<Vec<Box<dyn FnMut(WindowEvent) -> bool + Send>>>>,
}

pub enum UserEvent {
	Quit,
}
