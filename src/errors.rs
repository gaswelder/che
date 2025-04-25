use crate::buf::Pos;

pub struct Error {
    pub message: String,
    pub pos: Pos,
}

pub struct TWithErrors<T> {
    pub obj: T,
    pub errors: Vec<Error>,
}
