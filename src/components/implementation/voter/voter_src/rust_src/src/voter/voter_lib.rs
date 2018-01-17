use std::str::FromStr;
use lib_composite::sl::{ThreadParameter, Sl,Thread};
use lib_composite::sl_lock::Lock;
use std::mem;
use std::fmt;
use std::sync::Arc;
use std::ops::{DerefMut,Deref};
use voter::channel::*;

pub const MAX_REPS:usize = 3;

#[derive(Debug,PartialEq,Copy,Clone)]
pub enum ReplicaState {
	Init,        /* initialized not running */
	Processing,  /* processing */
	Read,        /* issued a read (blocked) */
	Written,     /* issued a write (blockd) */
	Done,        /* DEBUG - remove me */
}

pub enum VoteStatus {
	Fail(usize,u16), /* stores reference to divergent replica  comp id, rep id*/
	Inconclusive,
	Success,
}

//TODO - Fix public methods - i think only the channel will be exposed.

pub struct Replica {
	pub state:   ReplicaState,
	thd:         Option<Thread>,
	pub channel:  Option<Arc<Lock<Channel>>>,
	ret_val:     Option<i32>,
	pub unit_of_work: u16,
	pub rep_id:  u16,
}

pub struct ModComp {
	pub replicas:     Vec<Arc<Lock<Replica>>>,
	pub comp_id: usize,
	pub num_replicas: usize,
}

pub struct CompStore {
	pub components:Vec<ModComp>,
}

//manually implement as lib_composite::Thread doesn't impl debug
impl fmt::Debug for Replica {
	fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Replica: [rep_id - {} Thdid - {} State - {:?} ret_val - {:?}]",
        self.rep_id, self.get_thdid(), self.state, self.ret_val)
    }
}

impl fmt::Debug for VoteStatus {
	fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Status: {}",
        match self {
        	&VoteStatus::Inconclusive => String::from_str("Inconclusive").unwrap(),
        	&VoteStatus::Success => String::from_str("Success").unwrap(),
        	&VoteStatus::Fail(comp,rep) => format!("Fail - comp {:?} rep {:?}",comp,rep),
        })
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
	pub fn new(rep_id:u16) -> Replica  {
		Replica {
			state : ReplicaState::Init,
			thd: None,
			channel: None,
			ret_val : None,
			unit_of_work: 0,
			rep_id,
		}
	}
	//using a setter to contol the initial scheduling of the thread.
	pub fn set_thd(&mut self,thd: Thread) {
		self.thd = Some(thd);
		self.state_transition(ReplicaState::Processing);
		self.thd.as_mut().expect("set_thd missing thd").set_param(ThreadParameter::Priority(5));
	}
	//really dont think we need this
	pub fn get_thdid(&self) -> u16 {
		match self.thd.as_ref() {
			Some(thd) => thd.thdid(),
			None      => 9999,
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

	pub fn block(rep: &Arc<Lock<Replica>>, sl:Sl) {
		assert!(rep.lock().deref().is_blocked());
		sl.block();
	}
	//TODO!
	pub fn recover(&mut self) {
		assert!(false);
	}

}

impl CompStore {
	pub fn new() -> CompStore {
		CompStore {
			components: Vec::new(),
		}
	}
}

impl ModComp {
	pub fn new(num_replicas: usize, comp_store:&mut CompStore, sl: Sl, thd_entry: fn(sl:Sl, rep: Arc<Lock<Replica>>)) -> usize {
		assert!(num_replicas <= MAX_REPS);
		//create new Component
		let mut comp = ModComp {
			replicas: Vec::with_capacity(num_replicas),
			comp_id: (comp_store.components.len()),
			num_replicas,
		};

		//create replicas,start their threads,add them to the componenet
		for i in 0..num_replicas {
			let rep = Arc::new(Lock::new(sl,Replica::new(i as u16)));
			let rep_ref = Arc::clone(&rep);

			let thd = sl.spawn(move |sl:Sl| {thd_entry(sl, rep_ref);});
			println!("Created thd {}",thd.thdid());
			rep.lock().deref_mut().set_thd(thd);
			comp.replicas.push(Arc::clone(&rep));
		}

		comp_store.components.push(comp);
		return comp_store.components.len() - 1;
	}

	pub fn wake_all(&mut self) {
		//update state first to avoid replicas beginning new read/write
		//while still in old read/write state
		for i in 0..self.num_replicas {
			let mut replica = self.replicas[i].lock();
			assert!(replica.deref().state != ReplicaState::Init);
			replica.deref_mut().state_transition(ReplicaState::Processing);
		}

		for replica in &self.replicas {
			replica.lock().deref_mut().thd.as_mut().unwrap().wakeup();
		}
	}

	pub fn collect_vote(& mut self) -> VoteStatus {
		println!("Component Collecting Votes");
		//check first to see that all replicas are done processing.
		for replica in &self.replicas {
			if replica.lock().deref().is_processing() {return VoteStatus::Inconclusive}
		}

		//VOTE!
		let mut consensus = [0,0,0];
		for replica in &mut self.replicas {
			let mut replica = replica.lock();
			replica.deref_mut().unit_of_work += 1;
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

		let faulted = self.failure_find(healthy_state);
		return VoteStatus::Fail(faulted.0,faulted.1)
	}

	fn failure_find(&self, healthy_state:ReplicaState) -> (usize,u16) {
		//TODO return replica that is not in healthy state
		return (0,0)
	}

	//TODO - this function needs to be called by the channel somewhere, send recieve ????
	fn replica_communicate(&mut self, sl:Sl, rep_id:usize, mut ch: Channel, comp_store:& mut CompStore, action:ReplicaState) -> Result<Option<i32>, String> {
		assert!(rep_id < self.num_replicas);
		let reader_id = if ch.reader_id.is_some() {ch.reader_id.unwrap()} else {return Err("replica_communicate fail, no reader".to_string())};
		let writer_id = if ch.reader_id.is_some() {ch.writer_id.unwrap()} else {return Err("replica_communicate fail, no writer".to_string())};

		if reader_id == self.comp_id && action != ReplicaState::Read ||
		   writer_id == self.comp_id && action != ReplicaState::Written {
		   	return Err(format!("Replica_communicate: Component not permitted to {:?}",action));
		}


		self.replicas[rep_id].lock().deref_mut().state_transition(action);

		loop {
			let (reader_result, writer_result) = ch.call_vote(comp_store)?;
			if self.check_vote_status(reader_result, comp_store, rep_id, sl) && self.check_vote_status(writer_result, comp_store, rep_id, sl) {break}
		}

		return Ok(self.replicas[rep_id].lock().deref_mut().retval_get());
	}

	fn check_vote_status(&mut self, vote:VoteStatus, comp_store:& mut CompStore, rep_id:usize, sl:Sl) -> bool {
		match vote {
			VoteStatus::Fail(comp,rep) => {
				comp_store.components[comp].replicas[rep as usize].lock().deref_mut().recover();
				false
			},
			VoteStatus::Inconclusive => {
				Replica::block(&self.replicas[rep_id],sl);
				true
			},
			VoteStatus::Success => {
				//TODO implemnt channel calls here
				true
			},
		}
	}
}


