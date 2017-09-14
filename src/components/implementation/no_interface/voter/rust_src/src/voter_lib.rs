extern crate lib_composite;
use lib_composite::sys::types::thdid_t;


const REPLICA_MAX: usize = 3;
const REPLICA_BUF_SZ: i32 = 1024;

#[derive(Debug)]
pub enum ReplicaState {
	ReplicaInit,		/* initialized not running */
	ReplicaProcessing,  /* processing */
	ReplciaRead,        /* issued a read (blocked) */
	ReplicaWritten,     /* issued a write (blockd) */
}

// as far as public goes i think only channel related things should be exposed beyond the api?? right 

#[derive(Debug)]
pub struct Replica {
	state:  ReplicaState,
	thd_id:  thdid_t, 
	buf:    Vec<char>,
	amnt:   i32,
	ret_val: Option<i32>,
}

/* Any component running through the voter will be managed by the 
 * modular component struct. This struct will manage the replicated
 * processes 
 */ 

#[derive(Debug)]
pub struct ModComp {
	replicas:  Vec<Replica>,
	num_replicas: i32,
}


struct Channel<'a>  {
	reader: &'a ModComp,
	writer: &'a ModComp,
}

impl Replica  {
	pub fn new(thd_id: thdid_t) -> Replica  {
		Replica {
			state : ReplicaState::ReplicaInit,
			thd_id,
			buf: Vec::new(),
			amnt : 0,
			ret_val : None,
		}
	}
}

impl ModComp {
	pub fn new(num_replicas: i32) -> ModComp {
		let mut comp = ModComp { 
			replicas: Vec::with_capacity(num_replicas as usize),
			num_replicas,
		};
		for i in 0..num_replicas {
			comp.replicas.push(Replica::new(0 as thdid_t))
		}

		comp
	}
}