#[derive(Clone, Copy)]
pub struct Symb<'a> {
    name: &'a str,
    addr: u64,
    stype: SymbType,
}

#[derive(PartialEq, Clone, Copy)]
pub enum SymbType {
    GlobalData,
    GlobalFn,
    Other,
}

impl<'a> Symb<'a> {
    pub fn new(name: &'a str, addr: u64, t: SymbType) -> Option<Symb<'a>> {
	if t == SymbType::Other {
	    return None;
	}

        Some(Symb {
            name: name,
            addr: addr,
            stype: t,
        })
    }
    pub fn name(&self) -> &'a str {
        self.name
    }
    pub fn addr(&self) -> u64 {
        self.addr
    }
    pub fn stype(&self) -> SymbType {
	self.stype
    }
}
