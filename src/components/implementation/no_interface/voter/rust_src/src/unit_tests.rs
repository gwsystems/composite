use voter_lib;
use voter_lib::ReplicaState::*;
use voter_lib::Replica;
use lib_composite::sl::{ThreadParameter, Sl};
use lib_composite::sl_lock::Lock;
use std::sync::Arc;
use std::ops::{DerefMut,Deref};
use std::time::Duration;


/*
/* ------------- Check to make sure states correctly transation on replicas ---------- */
pub fn test_state_logic(sl: Sl) {
	//QUESTION -- why do i need to decalre the whole component mutable, when i defined the individual replicas
	//as mutable
	let mut test_comp = voter_lib::ModComp::new(1,sl,thd_entry);
	test_comp.replicas[0].state_transition(Processing);
	printc!("Expceted: Replica Processing :: Actual : {:?}", test_comp.replicas[0].state);
	printc!("Expected: True :: Actual: {}",test_comp.replicas[0].is_processing());
	test_comp.replicas[0].state_transition(Written);
	printc!("Expected: True :: Actual: {}",test_comp.replicas[0].is_blocked());
}
fn thd_entry(sl:Sl, rep: Arc<Replica>) {printc!("hello")}
*/
/* --------------- put threads to sleep and see if they wake up ----------------- */

pub fn test_wakeup(sl: Sl, num_reps:usize) {
	let mut test_comp = voter_lib::ModComp::new(num_reps,sl,thd_block);
	printc!("Entering test function");
	let mut blocked_reps = 0;
	while blocked_reps < num_reps {
		sl.block_for(Duration::new(1,0));
		if test_comp.replicas[blocked_reps].lock().deref().is_blocked() {
			blocked_reps+=1;
		}
	}
	printc!("Voter waking all replicas!");
	test_comp.wake_all();
}

fn thd_block(sl:Sl, rep: Arc<Lock<Replica>>) {
	let rep_id = rep.lock().deref().get_thdid();
	printc!("thread {} running",rep_id);
	rep.lock().deref_mut().state_transition(Read);
	voter_lib::Replica::block(rep,sl);
	printc!("Thead {} awake!", rep_id);
}


 /* ------------ Test vote functions abillity to detect state differences -------------- */

pub fn test_vote_simple(sl: Sl) {
	let test_comp = voter_lib::ModComp::new(1,sl,vote_a);
	//printc!("Expected: Inconclusive; Actual: {:?}", test_comp.vote());

	// let test_comp = voter_lib::ModComp::new(3,sl,vote_b);
	// printc!("Expected: Fail(Read); Actual: {:?}", test_comp.vote());

	// let test_comp = voter_lib::ModComp::new(2,sl,vote_c);
	// printc!("Expected: Success; Actual: {:?}", test_comp.vote());
}

fn vote_a(sl:Sl, rep: Arc<Lock<Replica>>) {
	printc!("replica running");
	printc!("thread has {:?}",rep.lock().deref());
	sl.block();
}
/*
fn vote_b(sl:Sl, rep: Arc<Replica>) {
	printc!("replica running");
	if rep.get_thdid() < 1 {
		rep.block(sl,Written)
	}
	else {
		rep.block(sl,Read)
	}
}
fn vote_c(sl:Sl, rep: Arc<Replica>) {
	printc!("replica running");
	rep.block(sl,Written)
}
*/

/* -------------- Test lib Composite chagnes ------------------- */

pub fn test_lib_composite(sl:Sl) {
	let mut thd = sl.spawn(move |sl:Sl| {printc!("thd started")});
	thd.set_param(ThreadParameter::Priority(5));
	printc!("Thread id {}", thd.thdid());
	printc!("Current Thread {}", sl.current_thread().thdid());
}