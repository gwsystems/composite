use voter_lib;
use voter_lib::ReplicaState::*;
use voter_lib::Replica;
use lib_composite::sl::{ThreadParameter, Sl};
use lib_composite::sl_lock::Lock;
use std::sync::Arc;
use std::ops::{DerefMut,Deref};
use std::time::Duration;

fn wait_for_done(comp:voter_lib::ModComp,sl:Sl) {
	let mut done = 0;
	while done < comp.num_replicas {
		if comp.replicas[done].lock().deref().state == Done {
			done+=1;
		}
		else {
			sl.block_for(Duration::new(1,0));
		}
	}
}

fn wait_for_blocked(comp:&voter_lib::ModComp,sl:Sl) {
	let mut blocked_reps = 0;
	while blocked_reps < comp.num_replicas {
		sl.block_for(Duration::new(1,0));
		if comp.replicas[blocked_reps].lock().deref().is_blocked() {
			blocked_reps+=1;
		}
	}
}

fn test_print(msg:&'static str) {
	println!();
	println!("========================================");
	println!("{}",msg);
	println!("========================================");
}


/* ------------- Check to make sure states correctly transation on replicas ---------- */
pub fn test_state_logic(sl: Sl,num_reps:usize) {
	test_print("Begin state logic test");
	let mut test_comp = voter_lib::ModComp::new(num_reps,sl,thd_entry);
	wait_for_done(test_comp,sl);
}

fn thd_entry(sl:Sl, rep: Arc<Lock<Replica>>) {
	println!("Running {:?}",rep.lock().deref());

	println!("Expceted: Processing :: Actual : {:?}", rep.lock().deref().state);
	println!("Expected: True :: Actual {}", rep.lock().deref().is_processing());

	rep.lock().deref_mut().state_transition(Written);
	println!("Expceted: Written :: Actual : {:?}", rep.lock().deref().state);

	println!("Is_blocked Expected: True :: Actual: {}",rep.lock().deref().is_blocked());
	rep.lock().deref_mut().state_transition(Processing);

	println!("Rep ret value - {:?}", rep.lock().deref_mut().retval_get());

	rep.lock().deref_mut().state_transition(Done);

}

/* --------------- put threads to sleep and see if they wake up ----------------- */

pub fn test_wakeup(sl: Sl, num_reps:usize) {
	test_print("Begin thread sleep and component wake test");
	let mut test_comp = voter_lib::ModComp::new(num_reps,sl,thd_block);
	wait_for_blocked(&test_comp,sl);
	println!("Voter waking all replicas!");
	test_comp.wake_all();
}

fn thd_block(sl:Sl, rep: Arc<Lock<Replica>>) {
	println!("Running {:?}",rep.lock().deref());
	let rep_id = rep.lock().deref().get_thdid();
	rep.lock().deref_mut().state_transition(Read);
	voter_lib::Replica::block(&rep,sl);
	println!("Awake {:?}",rep.lock().deref());
}


 /* ------------ Test vote functions abillity to detect state differences -------------- */

pub fn test_vote_simple(sl: Sl, num_reps:usize) {
	test_print("Begin vote collect state test");
	let test_comp = voter_lib::ModComp::new(num_reps,sl,vote_a);
	println!("Expected: Inconclusive; Actual: {:?}", test_comp.collect_vote());

	let test_comp_2 = voter_lib::ModComp::new(num_reps,sl,vote_b);
	sl.block_for(Duration::new(2,0));
	println!("Expected: Fail; Actual: {:?}", test_comp_2.collect_vote());


	let test_comp3 = voter_lib::ModComp::new(num_reps,sl,vote_c);
	sl.block_for(Duration::new(2,0));
	println!("Expected: Success; Actual: {:?}", test_comp3.collect_vote());
}

fn vote_a(sl:Sl, rep: Arc<Lock<Replica>>) {
	println!("Running {:?}",rep.lock().deref());
}

fn vote_b(sl:Sl, rep:  Arc<Lock<Replica>>) {
	println!("Running {:?}",rep.lock().deref());
	if rep.lock().deref().rep_id < 1 {
		rep.lock().deref_mut().state_transition(Written);
	}
	else {
		rep.lock().deref_mut().state_transition(Read);
	}
}


fn vote_c(sl:Sl, rep:  Arc<Lock<Replica>>) {
	println!("Running {:?}",rep.lock().deref());
	rep.lock().deref_mut().state_transition(Written);
}


/* -------------------- Test Channel  ------------------------ */
pub fn test_channel_create(sl:Sl,num_reps:usize) {
	test_print("Begin Chanel test");
	let mut reader = voter_lib::ModComp::new(num_reps,sl,thd_block);
	let mut writer = voter_lib::ModComp::new(num_reps,sl,thd_block);
	let mut channel = voter_lib::Channel::new(& mut reader,& mut writer,sl);
	println!("Channle Created - {:?}", channel);
	sl.block_for(Duration::new(2,0));
	println!("channel wake");
	channel.wake_all();
	println!("Channel Calling Vote: Expected: Success :: Inconclusivet : {:?},",channel.call_vote());
}


/* -------------- Test lib Composite chagnes ------------------- */

pub fn test_lib_composite(sl:Sl) {
	let mut thd = sl.spawn(move |sl:Sl| {println!("thd started")});
	thd.set_param(ThreadParameter::Priority(5));
	println!("Thread id {}", thd.thdid());
	println!("Current Thread {}", sl.current_thread().thdid());
}
