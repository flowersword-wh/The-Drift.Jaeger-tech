pub mod default;
pub mod just_demo;

pub trait TestCase {
    fn prepare(&self) -> std::io::Result<()>;
    // fn run(&self, runner: &dyn Fn(&Path) -> std::io::Result<()>) -> std::io::Result<()>;
    fn clean(&self) -> std::io::Result<()>;
    // fn verify(&self, verify: &dyn Fn(&Path) -> std::io::Result<()>) -> std::io::Result<()>;
}
