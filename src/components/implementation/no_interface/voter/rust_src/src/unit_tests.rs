use voter_lib;
use voter_lib::ReplicaState::*;
use voter_lib::Replica;
use lib_composite::sl::{ThreadParameter, Sl};
use lib_composite::sl_lock::Lock;
use std::sync::Arc;
use std::ops::{DerefMut,Deref};
use std::time::Duration;

fn wait_for_done(comp:&voter_lib::ModComp,sl:Sl) {
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
	let mut components = voter_lib::CompStore::new();
	let mut test_comp_id = voter_lib::ModComp::new(num_reps,& mut components,sl,thd_entry);
	wait_for_done(&(components.components[test_comp_id]),sl);
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

// /* --------------- put threads to sleep and see if they wake up ----------------- */

pub fn test_wakeup(sl: Sl, num_reps:usize) {
	test_print("Begin thread sleep and component wake test");
	let mut compStore = voter_lib::CompStore::new();
	let mut test_comp_id = voter_lib::ModComp::new(num_reps,&mut compStore,sl,thd_block);
	wait_for_blocked(&(compStore.components[test_comp_id]),sl);
	sl.block_for(Duration::new(1,0));
	println!("Voter waking all replicas!");
	compStore.components[test_comp_id].wake_all();
}

fn thd_block(sl:Sl, rep: Arc<Lock<Replica>>) {
	println!("Running {:?}",rep.lock().deref());
	let rep_id = rep.lock().deref().get_thdid();
	rep.lock().deref_mut().state_transition(Read);
	voter_lib::Replica::block(&rep,sl);
	println!("Awake {:?}",rep.lock().deref());
}


//  /* ------------ Test vote functions abillity to detect state differences -------------- */

pub fn test_vote_simple(sl: Sl, num_reps:usize) {
	test_print("Begin vote collect state test");
	let mut compStore = voter_lib::CompStore::new();
	let test_comp = voter_lib::ModComp::new(num_reps,&mut compStore,sl,vote_a);
	println!("Expected: Inconclusive; Actual: {:?}", compStore.components[test_comp].collect_vote());

	let test_comp_2 = voter_lib::ModComp::new(num_reps,&mut compStore,sl,vote_b);
	sl.block_for(Duration::new(2,0));
	println!("Expected: Fail; Actual: {:?}", compStore.components[test_comp_2].collect_vote());


	let test_comp3 = voter_lib::ModComp::new(num_reps,&mut compStore,sl,vote_c);
	sl.block_for(Duration::new(2,0));
	println!("Expected: Success; Actual: {:?}", compStore.components[test_comp3].collect_vote());
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


// /* -------------------- Test Channel  ------------------------ */
pub fn test_channel_create(sl:Sl,num_reps:usize) {
	test_print("Begin Chanel test");
	let mut compStore = voter_lib::CompStore::new();
	let mut reader_id = voter_lib::ModComp::new(num_reps,&mut compStore,sl,thd_block);
	let mut writer_id = voter_lib::ModComp::new(num_reps,&mut compStore,sl,thd_block);
	let mut channel = voter_lib::Channel::new(reader_id,writer_id,sl);
	println!("Channle Created - {:?}", channel);
	sl.block_for(Duration::new(2,0));
	println!("channel wake");
	channel.wake_all(&mut compStore);
	println!("Channel Calling Vote: Expected: Inconclusive : {:?},", channel.call_vote(&compStore));
}


