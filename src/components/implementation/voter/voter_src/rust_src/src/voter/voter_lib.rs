use std::str::FromStr;
use lib_composite::sl::{ThreadParameter, Sl,Thread};
use std::fmt;
use voter::*;

pub const MAX_REPS:usize = 3;
pub const BUFF_SIZE:usize = 16;

#[derive(Debug,PartialEq,Copy,Clone)]
pub enum ReplicaState {
	Init,        /* initialized not running */
	Processing,  /* processing */
	Blocked,
}
#[derive(PartialEq)]
pub enum VoteStatus {
	//fixme should be usize
	Fail(usize), /* stores reference to divergent replica rep id*/
	Inconclusive(u8,usize),
	Success,
}

pub struct Replica {
	pub rep_id:       usize,
	pub state:        ReplicaState,
	thd:              Thread,
	pub data_buffer: [u8;BUFF_SIZE],
}

pub struct Component {
	pub replicas:         Vec<Replica>,
	pub num_replicas:     usize,
	pub new_data:         bool,
}

//manually implement as lib_composite::Thread doesn't impl debug
impl fmt::Debug for Replica {
	fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Replica: [rep_id - {} Thdid - {} State - {:?}]",
        self.rep_id, self.thd.thdid(), self.state)
    }
}

impl fmt::Debug for VoteStatus {
	fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Status: {}",
        match self {
        	&VoteStatus::Inconclusive(num_processing,rep) => format!("Inconclusive {}:{:?}", num_processing, rep),
        	&VoteStatus::Success => String::from_str("Success").unwrap(),
        	&VoteStatus::Fail(rep) => format!("Fail - {:?}",rep),
        })
    }
}

//manually implement as lib_composite::Lock doenst impl debug
impl fmt::Debug for Component {
	fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
		//dont want to spend the resources to lock and print each replica
		write!(f, "Component: num_replicas:{}", self.num_replicas)
	}
}

impl Replica  {
	pub fn new(rep_id:usize,mut thd:Thread) -> Replica  {
		thd.set_param(ThreadParameter::Priority(REP_PRIO));
		Replica {
			state : ReplicaState::Processing,
			thd: thd,
			rep_id,
			data_buffer: [0;BUFF_SIZE],
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
		return self.state == ReplicaState::Blocked;
	}

	pub fn is_processing(&self) -> bool {
		return self.state == ReplicaState::Processing;
	}

	//TODO!
	pub fn recover(&mut self) {
		panic!("Replica {:?} must be recovered", self.rep_id);;
	}

	pub fn write(&mut self, data:[u8;BUFF_SIZE]) {
		for i in 0..BUFF_SIZE {
			self.data_buffer[i] = data[i];
		}
	}
}

impl Component {
	pub fn new(num_replicas: usize, sl: Sl, thd_entry: fn(sl:Sl, rep_id: usize)) -> Component {
		assert!(num_replicas <= MAX_REPS);
		//create new Component
		let mut comp = Component {
			replicas: Vec::with_capacity(num_replicas),
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
			if replica.is_blocked() {replica.state_transition(ReplicaState::Processing);}
		}

		for replica in &mut self.replicas {
			replica.thd.wakeup();
		}
	}

	pub fn collect_vote(& mut self) -> VoteStatus {
		println!("Collecting Votes");
		let mut processing_rep_id = 0;
		let mut num_processing = 0;
		for replica in &self.replicas {
			if replica.is_processing() {
				num_processing += 1;
				//rep id is only useful in the case where only 1 replica is still processing
				//so it would only be set once in that case.
				processing_rep_id = replica.rep_id;
			}
		}
		//if any of the replicas are still processing bail.
		if num_processing > 0 {return VoteStatus::Inconclusive(num_processing,processing_rep_id)}

		//check the request each replica has made
		if !self.validate_msgs() {
			let faulted = self.find_faulted_msg();
			assert!(faulted>-1);
			return VoteStatus::Fail(faulted as usize);
		}

		return VoteStatus::Success;
	}

	pub fn validate_msgs(&self) -> bool {
		//compare each message against the first to look for difference (handle detecting fault later)
		let ref msg = &self.replicas[0].data_buffer;
		for replica in &self.replicas {
			if !compare_msgs(msg,&replica.data_buffer) {return false}
		}

		true
	}

	//todo return result
	pub fn find_faulted_msg(&self) -> i16 {
		//store the number of replicas that agree, and rep id of sender
		let mut concensus: [u8; MAX_REPS] = [0; MAX_REPS];

		//find which replica disagrees with the majority
		for i in 0..self.num_replicas {
			let msg_a = &self.replicas[i].data_buffer;
			for j in 0..self.num_replicas {
				if i == j {continue}

				let msg_b = &self.replicas[j].data_buffer;

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

pub fn compare_msgs(msg_a:&[u8;voter_lib::BUFF_SIZE], msg_b:&[u8;voter_lib::BUFF_SIZE]) -> bool {
	for i in 0..voter_lib::BUFF_SIZE {
		if msg_a[i] != msg_b[i] {return false}
	}

	true
}

