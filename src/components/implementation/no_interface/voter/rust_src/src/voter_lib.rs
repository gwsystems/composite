use lib_composite::sl::{ThreadParameter, Sl,Thread};
use std::mem;
use std::fmt;
use std::sync::{Arc,Mutex};

#[derive(Debug,PartialEq,Copy,Clone)]
pub enum ReplicaState {
	Init,        /* initialized not running */
	Processing,  /* processing */
	Read,        /* issued a read (blocked) */
	Written,     /* issued a write (blockd) */
}

#[derive(Debug)]
pub enum VoteStatus {
	Fail(ReplicaState), /* stores 'healthy' state, any replica not in this state kill */ 
	Inconclusive,
	Success,
}

//TODO - Fix public methods - i think only the channel will be exposed.

pub struct Replica {
	pub state:  ReplicaState,
	thd:        Option<Thread>,
	buf:        Vec<char>,
	amnt:       i32,
	ret_val:    Option<i32>,
}

#[derive(Debug)]
pub struct ModComp {
	pub replicas:     Vec<Arc<Mutex<Replica>>>,
	pub num_replicas: usize,
}


struct Channel<'a>  {
	reader: &'a ModComp,
	writer: &'a ModComp,
}

//manually implement as lib_composite::Thread doesn't impl debug
impl fmt::Debug for Replica {
	fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Replica: [Thdid - {} State - {:?} ret_val - {:?}]",
        match self.thd.as_ref() {
			Some(thd) => thd.thdid(),
			None      => 0,
		}, self.state, self.ret_val)
    }
}

impl Replica  {
	pub fn new() -> Replica  {
		Replica {
			state : ReplicaState::Init,
			thd: None,
			buf: Vec::new(),
			amnt : 0,
			ret_val : None,
		}
	}

	pub fn set_thd(&mut self, mut thd: Thread) {
		thd.set_param(ThreadParameter::Priority(5));
		self.thd = Some(thd);
		self.state_transition(ReplicaState::Processing);
	}

	pub fn get_thdid(&self) -> u16 {
		match self.thd.as_ref() {
			Some(thd) => thd.thdid(),
			None      => 0,
		}
	}

	pub fn state_transition(&mut self, state: ReplicaState) {
		println!("hitme 3");
		assert!(self.state != state);
		assert!(state != ReplicaState::Init);
		if self.is_blocked() {assert!(state == ReplicaState::Processing)}
		self.state = state; 
	}


	pub fn is_blocked(&self) -> bool {
		return self.state == ReplicaState::Read || self.state == ReplicaState::Written;
	}

	pub fn is_processing(&self) -> bool {
		return self.state == ReplicaState::Processing;
	}

	pub fn retval_get(&mut self) -> Option<i32> {
		mem::replace(&mut self.ret_val, None)
	}
	//confused when this would be called, wouldnt it just block the voter
	pub fn block(&mut self, sl:Sl, state: ReplicaState) {
		assert!(state == ReplicaState::Read || state == ReplicaState::Written);
		self.state_transition(state);
		assert!(self.is_blocked());
		println!("time to block!");
		sl.block(); 
	}

}

impl ModComp {
	//FIXME - Threads faulting when accesing the modComp, dont think the data is being 
	//correctly shared between threads. 
	pub fn new(num_replicas: usize,sl: Sl, thd_entry: fn(sl:Sl, rep: Arc<Mutex<Replica>>)) -> ModComp {
		//create new Component
		let mut comp = ModComp { 
			replicas: Vec::with_capacity(num_replicas),
			num_replicas,
		};

		//create replicas,start their threads,add them to the componenet 
		for i in 0..num_replicas {
			let rep = Arc::new(Mutex::new(Replica::new()));
			let thd = sl.spawn(|sl:Sl| {thd_entry(sl, Arc::clone(&rep));});
			println!("A");
			println!("What thread id are we getting! {}",thd.thdid());
			println!("B");
			rep.lock().unwrap().set_thd(thd);
			comp.replicas.push(Arc::clone(&rep));  
		}
		comp
	}

	pub fn wake_all(&mut self) {
		//udate state first to avoid replicas beginning new read/write 
		//while still in old read/write state
		for i in 0..self.num_replicas {
			let mut replica = self.replicas[i].lock().unwrap();
			assert!(replica.state != ReplicaState::Init);
			replica.state_transition(ReplicaState::Processing);
		}

		for replica in &self.replicas {
			println!("time to wake!");
			//replica.thd.wakeup();
			//println!("Replica to wake: {:?}",replica);
		}
	}

	pub fn vote(&self) -> VoteStatus {
		//check first to see that all replicas are done processing. 
		for replica in &self.replicas {
			if replica.lock().unwrap().is_processing() {return VoteStatus::Inconclusive}
		}

		//VOTE!
		let mut consensus = [0,0,0];  
		for replica in &self.replicas {
			let replica = replica.lock().unwrap();
			match replica.state {
				ReplicaState::Processing  => consensus[0] += 1,
				ReplicaState::Read        => consensus[1] += 1,
				ReplicaState::Written     => consensus[2] += 1,
				_ 						  => panic!("Invalid replica state in Vote"),
			}
		}

		//Check Consensus of Vote - if all same success! if conflict find the majority state 
		let mut max = 0;
		for i in 0..consensus.len() {
			if consensus[i] == self.num_replicas as i32 {return VoteStatus::Success}
			if consensus[i] > consensus[max] {max = i} 
		}

		return VoteStatus::Fail(match max {
			0 => ReplicaState::Processing,
			1 => ReplicaState::Read,
			2 => ReplicaState::Written,
			_ => panic!("Vote consensus max out of bounds"),
		})
	}
}


