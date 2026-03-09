#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct ContainerId(pub String);

impl ContainerId {
    pub fn as_str(&self) -> &str {
        &self.0
    }
}
