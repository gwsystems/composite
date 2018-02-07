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

 	fn transfer(&mut self, from_app:bool) {
 		{
	 		let (mut from,mut to) = if from_app {
	 			(&self.application,&mut self.service_provider)
	 		} else {
	 			(&self.service_provider,&mut self.application)
	 		};

			let msg = from.replicas[0].data_buffer;
			for replica in &mut to.replicas {
				for i in 0..voter_lib::BUFF_SIZE {
					replica.data_buffer[i] = msg[i];
				}
			}
		}

		let mut from = if from_app {&mut self.application} else {&mut self.service_provider};

		for replica in &mut from.replicas {
			for i in 0..voter_lib::BUFF_SIZE {
				replica.data_buffer[i] = 0;
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


			{
				let mut voter = voter_lock.lock();
				voter.transfer(true);
				voter.deref_mut().service_provider.wake_all();
			}

			//loop until service provider reaches consesus
			let mut srv_vote = voter_lib::VoteStatus::Fail(99);
			while srv_vote != voter_lib::VoteStatus::Success {
				{
					let mut voter = voter_lock.lock();
					srv_vote = Voter::vote(&mut voter.deref_mut().service_provider, sl);
				}
				if (srv_vote != voter_lib::VoteStatus::Success) {sl.thd_yield();} //todo cleanup
			}

			{
				let mut voter = voter_lock.lock();
				voter.transfer(false);
				voter.deref_mut().application.wake_all();
			}

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


	pub fn request(voter_lock:&Lock<Voter>, data:[u8;voter_lib::BUFF_SIZE], rep_id: usize, sl:Sl) -> [u8;voter_lib::BUFF_SIZE] {
		println!("Making Reqeust ....");
		{
			let mut voter = voter_lock.lock();
			voter.application.replicas[rep_id].write(data);
			voter.application.replicas[rep_id].state_transition(voter_lib::ReplicaState::Blocked);
		}
		sl.block();

		//get data returned from request.
		let voter = voter_lock.lock();
		let ref data = &voter.application.replicas[0].data_buffer;
		let mut msg = [0;voter_lib::BUFF_SIZE];

		for i in 0..voter_lib::BUFF_SIZE {
			msg[i] = data[i];
		}

		msg
	}

	pub fn get_reqeust(&mut self, rep_id: usize) -> [u8;voter_lib::BUFF_SIZE] {
		println!("Geting Request ....");

		let mut msg:[u8;voter_lib::BUFF_SIZE] = [0;voter_lib::BUFF_SIZE];
		{
			let mut buffer = &mut self.service_provider.replicas[rep_id].data_buffer;
			for i in 0..voter_lib::BUFF_SIZE {
				msg[i] = buffer[i];
				buffer[i] = 0;
			}
		}
		msg
	}

	pub fn send_response(voter_lock:&Lock<Voter>, data:[u8;voter_lib::BUFF_SIZE], rep_id: usize, sl:Sl) {
		println!("Sending Response ....");
		{
			let mut voter = voter_lock.lock();
			voter.service_provider.replicas[rep_id].write(data);
			voter.service_provider.replicas[rep_id].state_transition(voter_lib::ReplicaState::Blocked);
		}
		sl.block();
	}
}