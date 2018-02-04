pub mod voter_lib;

use lib_composite::sl_lock::Lock;
use lib_composite::sl::{ThreadParameter, Sl,Thread};
use std::ops::{DerefMut,Deref};

const MAX_CHANNELS:usize = 10;
const MAX_COMPS:usize = 5;

const REP_PRIO:u32 = 5;
const VOTE_PRIO:u32 = 1;

pub struct Voter {
	application:      voter_lib::Component,
	pub service_provider: voter_lib::Component,
}

impl Voter {
	pub fn new(app_reps:usize, srv_reps:usize, app_entry: fn(sl:Sl, rep_id:usize), srv_entry: fn(sl:Sl, rep_id:usize), sl:Sl) -> Voter {
		Voter {
			application:      voter_lib::Component::new(app_reps, sl, app_entry),
			service_provider: voter_lib::Component::new(srv_reps, sl, srv_entry),
		}
	}

 	fn transfer(&mut self) {
 		{
			let msg = &self.application.replicas[0].data_buffer;
			for srv_replica in &mut self.service_provider.replicas {
				for i in 0..voter_lib::BUFF_SIZE {
					srv_replica.data_buffer[i] = msg[i];
				}
			}
		}
		//clearing in seperate loop incase num app != num srv
		for app_replica in &mut self.application.replicas {
			for i in 0..voter_lib::BUFF_SIZE {
				app_replica.data_buffer[i] = 0;
			}
		}
	}

	pub fn monitor_vote(voter_lock:&Lock<Voter>, sl:Sl) {
		//todo handle Inconclusive
		loop {
			//loop until application reaches consensus.
			let mut application_vote = voter_lib::VoteStatus::Fail(99);
			while application_vote != voter_lib::VoteStatus::Success {
				{
					let mut voter = voter_lock.lock();
					application_vote = Voter::vote(&mut voter.deref_mut().application, sl);
				}
				if (application_vote != voter_lib::VoteStatus::Success) {sl.thd_yield();} //todo cleanup
			}

			voter_lock.lock().deref_mut().transfer();
			voter_lock.lock().deref_mut().service_provider.wake_all();

			//loop until service provider reaches consesus
			let mut srv_vote = voter_lib::VoteStatus::Fail(99);
			while srv_vote != voter_lib::VoteStatus::Success {
				{
					let mut voter = voter_lock.lock();
					srv_vote = Voter::vote(&mut voter.deref_mut().service_provider, sl);
				}
				if (srv_vote != voter_lib::VoteStatus::Success) {sl.thd_yield();} //todo cleanup
			}

			voter_lock.lock().deref_mut().application.wake_all();
		}
	}

	fn vote(comp:&mut voter_lib::Component, sl:Sl) -> voter_lib::VoteStatus {
		//take a higher priority for the duration of the vote.
		sl.current_thread().set_param(ThreadParameter::Priority(VOTE_PRIO));

		let vote = comp.collect_vote();
		match vote {
			voter_lib::VoteStatus::Fail(rep) => {
				comp.replicas[rep as usize].recover();
			},
			_ => (), //todo can this be cleaner?
		}
		println!("vote {:?}", vote);
		//lower priority and add vote thd back to the replica run queue
		sl.current_thread().set_param(ThreadParameter::Priority(REP_PRIO));
		vote
	}


	pub fn request(&mut self, data:[u8;voter_lib::BUFF_SIZE], rep_id: usize) {
		println!("Sending ....");
		self.application.replicas[rep_id].write(data);
		self.application.replicas[rep_id].state_transition(voter_lib::ReplicaState::Blocked);
		//small issue - cant block here becasue this will hold the lock to the voter struct.
	}

	pub fn service(&mut self, rep_id: usize) -> [u8;voter_lib::BUFF_SIZE] {
		println!("Reading ....");

		let mut msg:[u8;voter_lib::BUFF_SIZE] = [0;voter_lib::BUFF_SIZE];
		{
			let mut buffer = &mut self.service_provider.replicas[rep_id].data_buffer;
			for i in 0..voter_lib::BUFF_SIZE {
				msg[i] = buffer[i];
				buffer[i] = 0;
			}
		}
		self.service_provider.replicas[rep_id].state_transition(voter_lib::ReplicaState::Blocked);
		msg
	}
}