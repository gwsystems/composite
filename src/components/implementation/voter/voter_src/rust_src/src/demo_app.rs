use voter::voter_lib::ReplicaState::*;
use voter::voter_lib::Replica;
use lib_composite::sl::Sl;
use lib_composite::sl_lock::Lock;
use std::ops::{DerefMut,Deref};

use std::time::Duration;

use voter;
use voter::Voter;
use lazy_static;

//todo - figure out blocking design. is it okay to have the blocking responsibility live here????

lazy_static! {
	static ref VOTER:Lock<Voter> = unsafe {
		Lock::new(Sl::assert_scheduler_already_started(),
			      voter::Voter::new(3,1,do_work,service,Sl::assert_scheduler_already_started())
	)};
}

pub fn start(sl:Sl) {
	println!("Test app initializing");
	voter::Voter::monitor_vote(&*VOTER, sl);
}

/************************ Application *************************/
//todo - add fault cases: diff system calls, one doesnt make sys call, inf loop ...

fn do_work(sl:Sl, rep_id: usize) {
	println!("Replica {:?} starting work ....", rep_id);
	let mut i = 0;
	loop {
		 if i % 100 == 0 {
			make_systemcall(3,rep_id,sl);
			println!("Replica {:?} resuming work ....", rep_id);
		}
		i+=1;
	}
}

fn make_systemcall(sys_call:u8, rep_id:usize, sl:Sl) {
	println!("Replica {:?} making syscall {:?}", rep_id, sys_call);

	let mut data:[u8;voter::voter_lib::BUFF_SIZE] = [sys_call;voter::voter_lib::BUFF_SIZE];
	println!("Rep got {:?}",voter::Voter::request(&*VOTER,data,rep_id,sl));
	println!("rep {} blocking ...",rep_id );
}

/******************* Service Provider ********************/

fn service(sl:Sl, rep_id: usize) {
		//worried about how we set state... dont know how to start this in the blocked state.
		//HACK -- need a real solution around here.
	{
		let mut voter = VOTER.lock();
		voter.service_provider.replicas[rep_id].state_transition(voter::voter_lib::ReplicaState::Blocked);
	}
	sl.block();
	loop {
		println!("Service provider waking .... ");
		let msg = VOTER.lock().deref_mut().get_reqeust(rep_id);
		println!("performing system call {:?}", msg);
		let data = [9;voter::voter_lib::BUFF_SIZE];
		voter::Voter::send_response(&*VOTER,data,rep_id,sl);
	}
}
