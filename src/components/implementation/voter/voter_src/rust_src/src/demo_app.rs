use voter::voter_lib::ReplicaState::*;
use voter::voter_lib::Replica;
use lib_composite::sl::Sl;
use lib_composite::sl_lock::Lock;
use std::ops::{DerefMut,Deref};

use std::time::Duration;

use voter;
use voter::Voter;
use lazy_static;

lazy_static! {
	static ref VOTER:Lock<Voter> = unsafe {
		Lock::new(Sl::assert_scheduler_already_started(),
			      voter::Voter::new(3,1,do_work,service,Sl::assert_scheduler_already_started())
	)};
}

const MODE:u8 = 0;

pub fn start(sl:Sl) {
	println!("Test app initializing in mode {}",MODE);
	voter::Voter::monitor_components(&*VOTER, sl);
}

/************************ Application *************************/
fn do_work(sl:Sl, rep_id: usize) {
	println!("Replica {:?} starting work ....", rep_id);
	match MODE {
		0 => _healthy(sl,rep_id),
		1 => _stuck(sl,rep_id),
		2 => _bad_state(sl,rep_id),
		_ => panic!("Invalid mode"),
	}
}

fn _healthy(sl:Sl, rep_id: usize) {
	let mut i = 0;
	loop {
		if i % 100 == 0 {
			make_systemcall(i/100,rep_id,sl);
			println!("Replica {:?} resuming work ....", rep_id);
		}
		i+=1;
	}
}

fn _stuck(sl:Sl, rep_id: usize) {
	let mut i = 0;
	loop {
		if rep_id == 0 {continue;}
		if i % 100 == 0 {
			make_systemcall(i/100,rep_id,sl);
			println!("Replica {:?} resuming work ....", rep_id);
		}
		i+=1;
	}
}

fn _bad_state(sl:Sl ,rep_id: usize) {
	let mut i = 0;
	loop {
		 if i % 100 == 0 {
		 	if rep_id == 0 {
		 		make_systemcall(0,rep_id,sl);
		 	}
		 	else {
				make_systemcall(1,rep_id,sl);
		 	}
			println!("Replica {:?} resuming work ....", rep_id);
		}
		i+=1;
	}
}

fn make_systemcall(sys_call:u8, rep_id:usize, sl:Sl) {
	println!("Replica {:?} making syscall {:?}", rep_id, sys_call);

	let mut data:[u8;voter::voter_lib::BUFF_SIZE] = [sys_call;voter::voter_lib::BUFF_SIZE];
	println!("Rep got {:?}",voter::Voter::request(&*VOTER,data,rep_id,sl)[0]);
}

/******************* Service Provider ********************/

fn service(sl:Sl, rep_id: usize) {
	loop {
		let msg = voter::Voter::get_reqeust(&*VOTER, rep_id, sl);
		println!("performing system call {:?}", msg[0]);
		let data = [9;voter::voter_lib::BUFF_SIZE];
		voter::Voter::send_response(&*VOTER,data,rep_id,sl);
	}
}
