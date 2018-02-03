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
pub const WRITE_BUFF_SIZE:usize = 16;

#[derive(Debug,PartialEq,Copy,Clone)]
pub enum ReplicaState {
	Init,        /* initialized not running */
	Processing,  /* processing */
	Read,        /* issued a read (blocked) */
	Written,     /* issued a write (blockd) */
	Done,        /* DEBUG - remove me */
}
#[derive(PartialEq)]
pub enum VoteStatus {
	Fail(u16), /* stores reference to divergent replica rep id*/
	Inconclusive,
	Success,
}

pub struct Replica {
	pub state:    ReplicaState,
	thd:          Thread,
	pub channel:  Option<Arc<Lock<Channel>>>,
	ret_val:      Option<i32>,
	pub unit_of_work: u16,
	pub rep_id:  usize,
	pub write_buffer: [u8;WRITE_BUFF_SIZE],
}

pub struct ModComp {
	pub replicas:     Vec<Replica>, //TODO i think we can remove the arc lock, only voter thds have access
	pub comp_id: usize,
	pub num_replicas: usize,
	pub new_data:bool,
}

//manually implement as lib_composite::Thread doesn't impl debug
impl fmt::Debug for Replica {
	fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Replica: [rep_id - {} Thdid - {} State - {:?} ret_val - {:?}]",
        self.rep_id, self.thd.thdid(), self.state, self.ret_val)
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
	pub fn new(rep_id:usize,mut thd:Thread) -> Replica  {
		thd.set_param(ThreadParameter::Priority(5));
		Replica {
			state : ReplicaState::Processing,
			thd: thd,
			channel: None,
			ret_val : None,
			unit_of_work: 0,
			rep_id,
			write_buffer: [0;WRITE_BUFF_SIZE],
		}
	}

	pub fn state_transition(&mut self, state: ReplicaState) {
		//println!("        rep {}: old {:?} new {:?}",self.rep_id,self.state,state);
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

	pub fn block(rep: &Replica, sl:Sl) {
		assert!(rep.is_blocked());
		sl.block();
	}
	//TODO!
	pub fn recover(&mut self) {
		panic!("Replica {:?} must be recovered", self.rep_id);;
	}

	pub fn write(&mut self, data:[u8;WRITE_BUFF_SIZE]) {
		for i in 0..WRITE_BUFF_SIZE {
			self.write_buffer[i] = data[i];
		}
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
			new_data:false,
		};

		//create replicas,start their threads,add them to the componenet
		for i in 0..num_replicas {
			let thd = sl.spawn(move |sl:Sl| {thd_entry(sl, i);});
			comp.replicas.push(Replica::new(i,thd));
		}

		comp
	}

	pub fn wake_all(&mut self) {
		//update state first to avoid replicas beginning new read/write
		//while still in old read/write state
		for replica in &mut self.replicas {
			assert!(replica.state != ReplicaState::Init);
			replica.state_transition(ReplicaState::Processing);
		}

		for replica in &mut self.replicas {
			replica.thd.wakeup();
		}
	}

	pub fn collect_vote(& mut self) -> VoteStatus {
		println!("Component {} Collecting Votes", self.comp_id);
		//check first to see that all replicas are done processing.
		for replica in &self.replicas {
			if replica.is_processing() {return VoteStatus::Inconclusive}
		}

		//VOTE!
		let mut consensus = [0,0,0];
		for mut replica in &mut self.replicas {
			println!("Replica {}:{} - {:?}", self.comp_id,replica.rep_id,replica.state);
			match replica.deref().state {
				ReplicaState::Processing  => consensus[0] += 1,
				ReplicaState::Read        => consensus[1] += 1,
				ReplicaState::Written     => consensus[2] += 1,
				_                         => panic!("Invalid replica state in Vote"),
			}
		}
		println!("{} Consensus {:?}",self.comp_id,consensus);
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

	pub fn replica_communicate(current_comp_id:usize, rep_id:usize, ch:&mut Channel, action:ReplicaState, sl:Sl) -> Result<(), String> {
		let compStore(ref comp_lock) = COMPONENTS[current_comp_id];

		//take lock - error check and update state
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
			component.replicas[rep_id].state_transition(action);
			component.new_data = true;
		}
		//initiate vote for channel and loop on failure
		loop {
			let (writer_result, reader_result) = ch.call_vote()?;
			println!("Reader result - {:?}\nWriter result - {:?}",reader_result,writer_result);
			//if ModComp::check_vote_pass(ch.reader_id.unwrap(), &reader_result, rep_id, sl) && ModComp::check_vote_pass(ch.writer_id.unwrap(), &writer_result, rep_id, sl) {

			match writer_result {
				VoteStatus::Success => { /*if all writers wrote wake reader */
					println!("Waking reader...");
					let compStore(ref reader_lock) = COMPONENTS[ch.reader_id.unwrap()];
					let mut reader = reader_lock.lock();
					reader.deref_mut().as_mut().unwrap().wake_all();
					break
				},
				VoteStatus::Fail(faulted_rep_id) => { /*if a rep faulted reboot it*/
					println!("Rep faulted {}",faulted_rep_id);
					let compStore(ref writer_lock) = COMPONENTS[ch.writer_id.unwrap()];
					let mut writer = writer_lock.lock();
					writer.deref_mut().as_mut().unwrap().replicas[faulted_rep_id as usize].recover();
				},
				VoteStatus::Inconclusive => break, /*if no concensus let them run*/
			}


		}

		return Ok(());
	}
	//todo - this might get changed, we really only care about checking the readers status for the 3-1 serv provider model
	fn check_vote_pass(comp_id:usize, vote:&VoteStatus, rep_id:usize, sl:Sl) -> bool {
		let compStore(ref comp_store_wrapper_lock) = COMPONENTS[comp_id];
		let mut comp_lock = comp_store_wrapper_lock.lock();
		let mut comp = comp_lock.deref_mut().as_mut().unwrap();

		match *vote {
			VoteStatus::Fail(rep) => {
				comp.replicas[rep as usize].recover();
				false
			},
			VoteStatus::Inconclusive => {
				Replica::block(&comp.replicas[rep_id],sl);
				true
			},
			VoteStatus::Success => {
				true
			},
		}
	}

	pub fn validate_msgs(&self) -> bool {
		//compare each message against the first to look for difference (handle detecting fault later)
		let ref msg = &self.replicas[0].write_buffer;
		for replica in &self.replicas {
			if !compare_msgs(msg,&replica.write_buffer) {return false}
		}

		true
	}

	//todo return result
	pub fn find_faulted_msg(&self) -> i16 {
		//store the number of replicas that agree, and rep id of sender
		let mut concensus: [u8; MAX_REPS] = [0; MAX_REPS];

		//find which replica disagrees with the majority
		for i in 0..self.num_replicas {
			let msg_a = &self.replicas[i].write_buffer;
			for j in 0..self.num_replicas {
				if i == j {continue}

				let msg_b = &self.replicas[j].write_buffer;

				if compare_msgs(msg_a,msg_b) {
					concensus[i] += 1;
				}
			}
		}
		//go through concensus to get the rep id that sent the msg with least agreement
		let mut min:u8 = 4;
		let mut faulted:i16 = -1;
		for (rep,msg_votes) in concensus.iter().enumerate() {
			if  *msg_votes < min {
				min = *msg_votes;
				faulted = rep as i16;
			}
		}
		return faulted;
	}
}

pub fn compare_msgs(msg_a:&[u8;voter_lib::WRITE_BUFF_SIZE], msg_b:&[u8;voter_lib::WRITE_BUFF_SIZE]) -> bool {
	for i in 0..voter_lib::WRITE_BUFF_SIZE {
		if msg_a[i] != msg_b[i] {return false}
	}

	true
}

