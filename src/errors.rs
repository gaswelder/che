use crate::buf::Pos;

pub struct BuildError {
    pub path: String,
    pub pos: String,
    pub message: String,
}

pub struct Error {
    pub message: String,
    pub pos: Pos,
}

pub struct TWithErrors<T> {
    pub obj: T,
    pub errors: Vec<Error>,
}
