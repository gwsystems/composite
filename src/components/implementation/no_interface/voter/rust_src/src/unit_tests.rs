use voter_lib;
use voter_lib::ReplicaState::*;
use voter_lib::Replica;
use lib_composite::sl::{ThreadParameter, Sl};
use std::sync::{Arc,Mutex};

/*
/* ------------- Check to make sure states correctly transation on replicas ---------- */
pub fn test_state_logic(sl: Sl) {
	//QUESTION -- why do i need to decalre the whole component mutable, when i defined the individual replicas
	//as mutable 
	let mut test_comp = voter_lib::ModComp::new(1,sl,thd_entry);
	test_comp.replicas[0].state_transition(Processing);
	println!("Expceted: Replica Processing :: Actual : {:?}", test_comp.replicas[0].state);
	println!("Expected: True :: Actual: {}",test_comp.replicas[0].is_processing());
	test_comp.replicas[0].state_transition(Written);
	println!("Expected: True :: Actual: {}",test_comp.replicas[0].is_blocked());
}
fn thd_entry(sl:Sl, rep: Arc<Replica>) {println!("hello")}

/* --------------- put threads to sleep and see if they wake up ----------------- */ 

pub fn test_wakeup(sl: Sl, num_reps:usize) {
	let mut test_comp = voter_lib::ModComp::new(num_reps,sl,thd_block);
	let mut all_blocked = true;
	loop {
		for i in 0..num_reps {
			all_blocked = true;
			if !test_comp.replicas[i].is_blocked() {
				all_blocked = false;
				break;
			}
		}
		if all_blocked {break}
	}

	test_comp.wake_all();
}

fn thd_block(sl:Sl, rep: Arc<Replica>) {
	println!("im thread");
	rep.block(sl,Read);
	println!("Thead awake!");
}

*/
 /* ------------ Test vote functions abillity to detect state differences -------------- */ 

pub fn test_vote_simple(sl: Sl) {
	let test_comp = voter_lib::ModComp::new(1,sl,vote_a);
	println!("Expected: Inconclusive; Actual: {:?}", test_comp.vote());

	// let test_comp = voter_lib::ModComp::new(3,sl,vote_b);
	// println!("Expected: Fail(Read); Actual: {:?}", test_comp.vote());

	// let test_comp = voter_lib::ModComp::new(2,sl,vote_c);
	// println!("Expected: Success; Actual: {:?}", test_comp.vote());
}

fn vote_a(sl:Sl, rep: Arc<Mutex<Replica>>) {
	println!("replica running");
	println!("thread has {:?}",rep.lock().unwrap());
	sl.block();
}
/*
fn vote_b(sl:Sl, rep: Arc<Replica>) {
	println!("replica running");
	if rep.get_thdid() < 1 {
		rep.block(sl,Written)
	}
	else {
		rep.block(sl,Read)
	}
}
fn vote_c(sl:Sl, rep: Arc<Replica>) {
	println!("replica running");	
	rep.block(sl,Written)
}
*/

/* -------------- Test lib Composite chagnes ------------------- */ 

pub fn test_lib_composite(sl:Sl) {
	let mut thd = sl.spawn(move |sl:Sl| {println!("thd started")});
	thd.set_param(ThreadParameter::Priority(5));
	println!("Thread id {}", thd.thdid());
	println!("Current Thread {}", sl.current_thread().thdid());
}