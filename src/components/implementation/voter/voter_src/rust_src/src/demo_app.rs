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
	static ref APP_ID:usize = unsafe {voter::voter_new_app_init(3,Sl::assert_scheduler_already_started(),do_work)};
	static ref SRV_ID:usize = unsafe {voter::voter_new_app_init(1,Sl::assert_scheduler_already_started(),service)};

}

pub fn start(sl:Sl) {
	println!("Test app initializing");
	voter::channel_join_writer(*CHAN_ID,*APP_ID);
	voter::channel_join_reader(*CHAN_ID,*SRV_ID);
	println!("App Comp id {} Srv Comp id {} chan id {}", *APP_ID,*SRV_ID,*CHAN_ID);
}

/************************ Application *************************/
//todo - add fault cases: diff system calls, one doesnt make sys call, inf loop ...

fn do_work(sl:Sl, rep_id: usize) {
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
	println!("replica {:?} making syscall {:?}", rep_id, sys_call);

	let mut data:Vec<u8> = Vec::new();
	data.push(sys_call);
	voter::channel_snd(data,*CHAN_ID,*APP_ID,rep_id,sl);
	voter::replica_processing(*APP_ID,rep_id);
}


/******************* Service Provider (FAKE RK) ********************/

fn service(sl:Sl, rep_id: usize) {
	let msg = voter::channel_rcv(*CHAN_ID,*SRV_ID,rep_id,sl);
	if msg.is_some() {
		println!("performing system call {:?}", msg.unwrap().message);
	}
	voter::channel_wake(*CHAN_ID);
	sl.block();
}
