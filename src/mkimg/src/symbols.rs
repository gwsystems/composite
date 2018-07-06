pub struct Symb<'a> {
    name: &'a str,
    addr: u64
}

impl<'a> Symb<'a> {
    pub fn new(name: &'a str, addr: u64) -> Symb<'a> {
        Symb { name: name, addr: addr }
    }
    pub fn name(&self) -> &'a str {
        self.name
    }
    pub fn addr(&self) -> u64 {
        self.addr
    }
}
