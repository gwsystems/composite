use voter::voter_lib::ReplicaState::*;
use voter::voter_lib::Replica;
use lib_composite::sl::Sl;
use lib_composite::sl_lock::Lock;
use std::sync::Arc;
use std::ops::{DerefMut,Deref};
use std::time::Duration;
use voter::channel::*;
use voter;
use lazy_static;

lazy_static! {
	static ref CHAN_ID:usize = unsafe {voter::channel_create(Sl::assert_scheduler_already_started())};
	static ref APP_ID:usize = unsafe {voter::voter_new_app_init(2,Sl::assert_scheduler_already_started(),do_work)};
	static ref SRV_ID:usize = unsafe {voter::voter_new_app_init(1,Sl::assert_scheduler_already_started(),service)};
	static ref INIT:Lock<bool> = unsafe {Lock::new(Sl::assert_scheduler_already_started(),false)};
}

pub fn start(sl:Sl) {
	println!("Test app initializing");
	println!("App Comp id {} Srv Comp id {} chan id {}", *APP_ID,*SRV_ID,*CHAN_ID);
	voter::channel_join_writer(*CHAN_ID,*APP_ID);
	voter::channel_join_reader(*CHAN_ID,*SRV_ID);
	println!("--------------");
	*INIT.lock().deref_mut() = true;
}

/************************ Application *************************/
//todo - add fault cases: diff system calls, one doesnt make sys call, inf loop ...

fn do_work(sl:Sl, rep_id: usize) {
	while !(INIT.lock().deref()) {sl.block_for(Duration::new(1,0));}

	println!("Replica {:?} starting work ....", rep_id);
	let mut i = 0;
	loop {
		if i % 100 == 0 {
			make_systemcall(1,rep_id,sl);
			println!("Replica {:?} resuming work ....", rep_id);
		}
		i+=1;
	}
}

fn make_systemcall(sys_call:u8, rep_id:usize, sl:Sl) {
	println!("Replica {:?} making syscall {:?}", rep_id, sys_call);

	let mut data:Vec<u8> = Vec::new();
	data.push(sys_call);
	voter::channel_snd(data,*CHAN_ID,*APP_ID,rep_id,sl);
}


/******************* Service Provider ********************/

fn service(sl:Sl, rep_id: usize) {
	//while !(INIT.lock().deref()) {sl.block_for(Duration::new(1,0));}

	 //fixme - dont start blocked once we sort out the channel posting issue
	loop {
		voter::state_trans(*SRV_ID,rep_id,voter::voter_lib::ReplicaState::Read); //Conisder adding new blocked state
		sl.block();
		println!("Service provider waking .... ");
		let msg = voter::channel_rcv(*CHAN_ID,*SRV_ID,rep_id,sl);
		if msg.is_some() {
			println!("performing system call {:?}", msg.unwrap().message);
		}
		//voter::channel_wake(*CHAN_ID);
		voter::writer_wake(*CHAN_ID);
		//voter::state_trans(*SRV_ID,rep_id,voter::voter_lib::ReplicaState::Processing);
	}
}
