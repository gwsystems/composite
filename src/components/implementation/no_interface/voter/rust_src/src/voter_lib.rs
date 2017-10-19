use lib_composite::sl::{ThreadParameter, Sl,Thread};
use lib_composite::sl_lock::Lock;
use std::mem;
use std::fmt;
use std::sync::Arc;
use std::ops::{DerefMut,Deref};

#[derive(Debug,PartialEq,Copy,Clone)]
pub enum ReplicaState {
	Init,        /* initialized not running */
	Processing,  /* processing */
	Read,        /* issued a read (blocked) */
	Written,     /* issued a write (blockd) */
}

pub enum VoteStatus<'a> {
	Fail(&'a Arc<Lock<Replica>>), /* stores reference to divergent replica */
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

pub struct ModComp {
	pub replicas:     Vec<Arc<Lock<Replica>>>,
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

//manually implement as lib_composite::Lock doenst impl debug
impl fmt::Debug for ModComp {
	fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
		//dont want to spend the resources to lock and print each replica
		write!(f, "ModComp: num_replicas = {}", self.num_replicas)
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

	pub fn block(rep: Arc<Lock<Replica>>, sl:Sl) {
		assert!(rep.lock().deref().is_blocked());
		sl.block();
	}
	//TODO!
	pub fn recover(&mut self) {
		assert!(false);
	}

}

impl ModComp {
	pub fn new(num_replicas: usize,sl: Sl, thd_entry: fn(sl:Sl, rep: Arc<Lock<Replica>>)) -> ModComp {
		//create new Component
		printc!("Mod comp new called");
		let mut comp = ModComp {
			replicas: Vec::with_capacity(num_replicas),
			num_replicas,
		};

		//create replicas,start their threads,add them to the componenet
		for i in 0..num_replicas {
			let rep = Arc::new(Lock::new(sl,Replica::new()));
			let thd = sl.spawn(|sl:Sl| {thd_entry(sl, Arc::clone(&rep));});
			printc!("Created thd {}",thd.thdid());
			rep.lock().deref_mut().set_thd(thd);
			comp.replicas.push(Arc::clone(&rep));
		}
		comp
	}

	pub fn wake_all(&mut self) {
		printc!("Entering Wake");
		//update state first to avoid replicas beginning new read/write
		//while still in old read/write state
		for i in 0..self.num_replicas {
			let mut replica = self.replicas[i].lock();
			assert!(replica.deref().state != ReplicaState::Init);
			replica.deref_mut().state_transition(ReplicaState::Processing);
		}

		for replica in &self.replicas {
			printc!("time to wake!");
			replica.lock().deref_mut().thd.as_mut().unwrap().wakeup();
		}
	}

	pub fn vote(&self) -> VoteStatus {
		//check first to see that all replicas are done processing.
		for replica in &self.replicas {
			if replica.lock().deref().is_processing() {return VoteStatus::Inconclusive}
		}

		//VOTE!
		let mut consensus = [0,0,0];
		for replica in &self.replicas {
			let replica = replica.lock();
			match replica.deref().state {
				ReplicaState::Processing  => consensus[0] += 1,
				ReplicaState::Read        => consensus[1] += 1,
				ReplicaState::Written     => consensus[2] += 1,
				_                         => panic!("Invalid replica state in Vote"),
			}
		}

		//Check Consensus of Vote - if all same success! if conflict find the majority state
		let mut max_id = 0;
		for i in 0..consensus.len() {
			if consensus[i] == self.num_replicas as i32 {return VoteStatus::Success}
			if consensus[i] > consensus[max_id] {max_id = i}
		}
		let healthy_state = match max_id {
			0 => ReplicaState::Processing,
			1 => ReplicaState::Read,
			2 => ReplicaState::Written,
			_ => panic!("No consensus in find healthy state"),
		};

		return VoteStatus::Fail(self.failure_find(healthy_state))
	}

	fn failure_find(&self, healthy_state:ReplicaState) -> &Arc<Lock<Replica>> {
		//TODO return replica that is not in healthy state
		return self.replicas.get(0).expect("Found no faulted replica");
	}

	fn replica_communicate(&mut self, sl:Sl, rep_id:usize, ch:Channel, action:ReplicaState, amnt:i32) -> Option<i32> {
		assert!(rep_id < self.num_replicas);
		if ch.reader as *const _ == self as *const _ && action != ReplicaState::Read ||
		   ch.writer as *const _ == self as *const _ && action != ReplicaState::Written {
		   	return Some(-1); //TODO more descriptive return valeus
		}


		self.replicas[rep_id].lock().deref_mut().state_transition(action);
		self.replicas[rep_id].lock().deref_mut().amnt = amnt;


		loop {
			match self.vote() { //FIXME cange to channel vote.
				VoteStatus::Fail(faulted) => {
					faulted.lock().deref_mut().recover();
					break;
				},
				VoteStatus::Inconclusive       => {
					//FIXME
					//This will block what ever thread called this, not the replica
					//... i assume we want this thread to go and block the thread associated
					//with that replica but i dotn think that is possbile in SL (or at least rust sl)
					//Replica::block(replica,sl); need way to block
					break;
				},
				VoteStatus::Success            => {
					//TODO implemnt channel calls here
					break;
				},
			}
		}

		return self.replicas[rep_id].lock().deref_mut().retval_get();
	}

}