// String buffer with functions for parsing.
pub struct Buf {
    s: Vec<char>,
    pos: usize,

    line: usize,
    col: usize,
    linelengths: Vec<usize>,
}

#[derive(Debug, Clone)]
pub struct Pos {
    pub line: usize,
    pub col: usize,
}

impl Pos {
    pub fn fmt(&self) -> String {
        format!("{}:{}", self.line, self.col)
    }
}

pub fn new(s: String) -> Buf {
    Buf {
        s: s.chars().collect(),
        pos: 0,

        line: 1,
        col: 1,
        linelengths: Vec::new(),
    }
}

impl Buf {
    pub fn ended(&self) -> bool {
        return !self.more();
    }

    pub fn more(&self) -> bool {
        self.pos < self.s.len()
    }

    pub fn pos(&self) -> Pos {
        Pos {
            line: self.line,
            col: self.col,
        }
    }

    pub fn peek(&self) -> Option<char> {
        if self.more() {
            return Some(self.at(self.pos));
        } else {
            return None;
        }
    }

    fn at(&self, p: usize) -> char {
        self.s[p]
    }

    pub fn get(&mut self) -> Option<char> {
        if !self.more() {
            return None;
        }

        let c = self.at(self.pos);
        self.pos += 1;
        if c == '\n' {
            self.linelengths.push(self.col);
            self.line += 1;
            self.col = 1;
        } else {
            self.col += 1;
        }
        return Some(c);
    }

    // pub fn unget(&mut self, c: char) {
    //     assert!(self.pos > 0);
    //     assert!(self.at(self.pos - 1) == c);
    //     self.pos -= 1;
    //     if c as char == '\n' {
    //         self.line -= 1;
    //         self.col = self.linelengths.pop().unwrap();
    //     } else {
    //         self.col -= 1;
    //     }
    // }

    // Skips any sequence of characters from the given string.
    // Returns the skipped string.
    pub fn read_set(&mut self, set: String) -> String {
        let mut s = String::new();
        loop {
            if !self.more() {
                break;
            }
            let next = self.peek().unwrap_or(0 as char);
            if !set.chars().position(|c| c == next).is_some() {
                break;
            }
            s.push(self.get().unwrap());
        }
        return s;
    }

    pub fn skip_literal(&mut self, literal: &str) -> bool {
        if !self.literal_follows(literal) {
            return false;
        }
        for _ in 0..literal.len() {
            self.get();
        }
        return true;
    }

    pub fn until_literal(&mut self, literal: &str) -> String {
        let mut s = String::new();
        while self.more() && !self.literal_follows(literal) {
            s.push(self.get().unwrap());
        }
        return s;
    }

    pub fn literal_follows(&self, s: &str) -> bool {
        if s.len() > self.s.len() - self.pos {
            return false;
        }
        let mut i = 0;
        for c in s.bytes() {
            if self.at(self.pos + i) != c as char {
                return false;
            }
            i += 1;
        }
        return true;
    }

    // Skips characters until the given character is encountered.
    // The given character is left unread.
    // Returns the skipped characters.
    pub fn skip_until(&mut self, ch: char) -> String {
        let mut s = String::new();
        while self.more() && self.peek().unwrap() != ch {
            s.push(self.get().unwrap());
        }
        return s;
    }

    // pub fn context(&self) -> String {
    //     let s = String::new();

    //     let start = if self.pos < 10 { 0 } else { self.pos - 10 };
    //     let end = if self.pos + 10 > self.s.len() {
    //         self.s.len()
    //     } else {
    //         self.pos + 10
    //     };

    //     format!(
    //         "{}[{}]{}",
    //         self.s[start..self.pos].iter().collect::<String>(),
    //         self.s[self.pos],
    //         self.s[self.pos..end].iter().collect::<String>()
    //     );
    //     return s;
    // }

    // pub fn fcontext(self) -> String {
    //     let n = 10;
    //     let s = String::from(format!("[{}]", self.s[self.pos]));
    //     s += &s[self.pos + 1..(n - self.pos - 1)];
    //     return s;
    //     // s = str_replace(array("\r", "\n", "\t"), array(
    //     // 	"\\r",
    //     // 	"\\n",
    //     // 	"\\t"
    //     // ), $s);
    //     // return $s;
    // }
}

#[cfg(test)]
mod tests {
    use super::new;
    #[test]
    fn ended() {
        let mut buf = new(String::from("1"));
        assert_eq!(buf.ended(), false);
        buf.get();
        assert_eq!(buf.ended(), true);
    }

    #[test]
    fn pos() {
        let mut buf = new(String::from("123\n456"));
        let seq = ["1:1", "1:2", "1:3", "1:4", "2:1", "2:2", "2:3"];
        for pos in &seq {
            assert_eq!(buf.pos().fmt(), pos.to_string());
            buf.get();
        }
    }
    #[test]
    fn peek() {
        let mut buf = new(String::from('1'));
        assert_eq!('1', buf.peek().unwrap());
        assert_eq!('1', buf.peek().unwrap());
        buf.get();
        assert_eq!(buf.peek().is_none(), true);
    }
    #[test]
    fn get() {
        let mut buf = new(String::from("123"));
        assert_eq!(buf.get().unwrap(), '1');
        assert_eq!(buf.get().unwrap(), '2');
        assert_eq!(buf.get().unwrap(), '3');
        assert_eq!(buf.get().is_none(), true);
    }
    // #[test]
    // fn unget() {
    //     let mut buf = new(String::from("123"));
    //     assert_eq!('1', buf.get().unwrap());
    //     assert_eq!('2', buf.get().unwrap());
    //     buf.unget('2');
    //     assert_eq!('2', buf.get().unwrap());
    //     assert_eq!('3', buf.get().unwrap());
    //     assert_eq!(true, buf.get().is_none());
    // }
    #[test]
    fn read_set() {
        let mut buf = new(String::from("aba123"));
        assert_eq!("", buf.read_set(String::from("1234567890")));
        assert_eq!("aba", buf.read_set(String::from("abcdef")));
        assert_eq!('1', buf.get().unwrap());
    }
    #[test]
    fn skip_literal() {
        let mut buf = new(String::from("abc123"));
        assert_eq!(false, buf.skip_literal("123"));
        assert_eq!(true, buf.skip_literal("abc"));
        assert_eq!('1', buf.get().unwrap());
    }
    #[test]
    fn until_literal() {
        let mut buf = new(String::from("abc123"));
        assert_eq!("abc", buf.until_literal("123"));
        assert_eq!('1', buf.get().unwrap());
    }
    #[test]
    fn skip_until() {
        let mut buf = new(String::from("abc123"));
        assert_eq!("abc", buf.skip_until('1'));
        assert_eq!("123", buf.skip_until('z'));
    }
    #[test]
    fn literal_follows() {
        let buf = new(String::from("abc123"));
        assert_eq!(true, buf.literal_follows("abc"));
        assert_eq!(false, buf.literal_follows("123"));
    }
}
