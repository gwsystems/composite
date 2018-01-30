use std::str::FromStr;
use lib_composite::sl::{ThreadParameter, Sl,Thread};
use lib_composite::sl_lock::Lock;
use std::mem;
use std::fmt;
use std::sync::Arc;
use std::ops::{DerefMut,Deref};
use voter::channel::*;
use voter::*;

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
	Fail(u16), /* stores reference to divergent replica rep id*/
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
	pub replicas:     Vec<Arc<Lock<Replica>>>, //TODO i think we can remove the arc lock, only voter thds have access
	pub comp_id: usize,
	pub num_replicas: usize,
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
        	&VoteStatus::Fail(rep) => format!("Fail - {:?}",rep),
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

impl ModComp {
	pub fn new(num_replicas: usize, new_comp_id: usize, sl: Sl, thd_entry: fn(sl:Sl, rep_id: usize)) -> ModComp {
		assert!(num_replicas <= MAX_REPS);
		//create new Component
		let mut comp = ModComp {
			replicas: Vec::with_capacity(num_replicas),
			comp_id: new_comp_id,
			num_replicas,
		};

		//create replicas,start their threads,add them to the componenet
		for i in 0..num_replicas {
			let rep = Arc::new(Lock::new(sl,Replica::new(i as u16)));
			let rep_ref = Arc::clone(&rep);

			let thd = sl.spawn(move |sl:Sl| {thd_entry(sl, i);});
			println!("Created thd {}",thd.thdid());
			rep.lock().deref_mut().set_thd(thd);
			comp.replicas.push(Arc::clone(&rep));
		}

		comp
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
		return VoteStatus::Fail(faulted)
	}

	fn failure_find(&self, healthy_state:ReplicaState) -> u16 {
		//TODO return replica that is not in healthy state
		return 0
	}

	pub fn replica_communicate(current_comp_id:usize, rep_id:usize, ch:&mut Channel, action:ReplicaState, sl:Sl) -> Result<Option<i32>, String> {
		let compStore(ref comp_lock) = COMPONENTS[current_comp_id];

		//take lock - error check and update state of replica
		{
			let ref mut comp = comp_lock.lock();
			if comp.is_none() {return Err(format!("communicate, no comp at {}",current_comp_id))}

			assert!(rep_id < comp.deref().as_ref().unwrap().num_replicas);
			let reader_id = if ch.reader_id.is_some() {ch.reader_id.unwrap()} else {return Err("replica_communicate fail, no reader".to_string())};
			let writer_id = if ch.reader_id.is_some() {ch.writer_id.unwrap()} else {return Err("replica_communicate fail, no writer".to_string())};

			if reader_id == current_comp_id && action != ReplicaState::Read ||
			   writer_id == current_comp_id && action != ReplicaState::Written {
			   	return Err(format!("Replica_communicate: Component not permitted to {:?}",action));
			}

			let ref mut component = comp.deref_mut().as_mut().unwrap();
			component.replicas[rep_id].lock().deref_mut().state_transition(action);
		}
		//initiate vote for channel and loop on failure
		loop {
			let (reader_result, writer_result) = ch.call_vote()?;
			if ModComp::check_vote_pass(ch.reader_id.unwrap(), &reader_result, rep_id, sl) && ModComp::check_vote_pass(ch.writer_id.unwrap(), &writer_result, rep_id, sl) {
				//if were everything has written we are ready to wake the writers.
				match writer_result {
					VoteStatus::Success => {
						let compStore(ref reader_lock) = COMPONENTS[ch.reader_id.unwrap()];
						let mut reader = reader_lock.lock();
						reader.deref_mut().as_mut().unwrap().wake_all();
					},
					_ => break,
				}

				break;
			}
		}

		let ref comp = comp_lock.lock();
		let ref mut component = comp.deref().as_ref().unwrap();
		return Ok(component.replicas[rep_id].lock().deref_mut().retval_get());
	}

	fn check_vote_pass(comp_id:usize, vote:&VoteStatus, rep_id:usize, sl:Sl) -> bool {
		let compStore(ref comp_store_wrapper_lock) = COMPONENTS[comp_id];
		let mut comp_lock = comp_store_wrapper_lock.lock();
		let mut comp = comp_lock.deref_mut().as_mut().unwrap();

		match *vote {
			VoteStatus::Fail(rep) => {
				comp.replicas[rep as usize].lock().deref_mut().recover();
				false
			},
			VoteStatus::Inconclusive => {
				Replica::block(&comp.replicas[rep_id],sl);
				true
			},
			VoteStatus::Success => {
				//TODO implemnt channel calls here
				true
			},
		}
	}
}


