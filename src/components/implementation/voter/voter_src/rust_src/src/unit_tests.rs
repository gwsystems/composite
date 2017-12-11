#![allow(dead_code)] /* DEBUG REMOVE THIS */
use voter_lib;
use voter_lib::ReplicaState::*;
use voter_lib::Replica;
use lib_composite::sl::{ThreadParameter, Sl};
use lib_composite::sl_lock::Lock;
use std::sync::Arc;
use std::ops::{DerefMut,Deref};
use std::time::Duration;
use channel::*;

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
	wait_for_done(&(compStore.components[test_comp_id]),sl);
}

fn thd_block(sl:Sl, rep: Arc<Lock<Replica>>) {
	println!("Running {:?}",rep.lock().deref());
	let rep_id = rep.lock().deref().get_thdid();
	rep.lock().deref_mut().state_transition(Read);
	voter_lib::Replica::block(&rep,sl);
	println!("Awake {:?}",rep.lock().deref());
	rep.lock().deref_mut().state_transition(Done);
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
	let mut channel = Channel::new(reader_id,writer_id,&mut compStore,sl);
	println!("Channle Created - {:?}", channel.lock().deref());
	sl.block_for(Duration::new(2,0));
	println!("channel wake");
	channel.lock().deref_mut().wake_all(&mut compStore);
	println!("Channel Calling Vote: Expected: Inconclusive : {:?},", channel.lock().deref_mut().call_vote(& mut compStore));
}


// /* -------------------- Test Channel snd rcv ------------------------ */
pub fn test_snd_rcv(sl:Sl,num_reps:usize) {
	test_print("Begin snd rcv test");
	let mut compStore = voter_lib::CompStore::new();
	let reader = voter_lib::ModComp::new(num_reps,&mut compStore,sl ,thd_rcv);
	let mut channel = Channel::new(
		voter_lib::ModComp::new(num_reps,&mut compStore,sl ,thd_send),
		reader,
		&mut compStore,
		sl
	);
	wait_for_done(&(compStore.components[reader]),sl);
}

fn thd_send(sl:Sl, rep:  Arc<Lock<Replica>>) {
	println!("Running {:?}",rep.lock().deref());
	while rep.lock().deref().channel.is_none() {
		sl.block_for(Duration::new(1,0));
	}

	let msg = String::from("Hello").into_bytes();
	let mut rep = rep.lock();
	let rep_id = rep.deref().rep_id;
	let unit_of_work = rep.deref().unit_of_work;
	let ref mut chan = rep.deref_mut().channel.as_mut().expect("Replica has no channel to send");
	println!("SND MSG {:?} FROM {:?}", msg,rep_id);
	chan.lock().deref_mut().send(msg,rep_id,unit_of_work);
}

fn thd_rcv(sl:Sl, rep:  Arc<Lock<Replica>>) {
	println!("Running {:?}",rep.lock().deref());
	while rep.lock().deref().channel.is_none() {
		sl.block_for(Duration::new(1,0));
	}
	let mut rep = rep.lock();
	while !rep.deref_mut().channel.as_mut().expect("rep has no chan").lock().deref().has_data() {
		sl.block_for(Duration::new(1,0));
	}
	let msg = rep.deref_mut().channel.as_mut().expect("rep has no chan").lock().deref_mut().receive().expect("no msg");
	println!("GOT MSG {:?} FROM {:?}",msg.message, msg.rep_id);
	rep.state_transition(Done);
}

// /* -------------------- Test Channel msg validation ------------------------ */

pub fn test_chan_validate(sl:Sl) {
	test_print("Begin channel msg validate test");
	let mut compStore = voter_lib::CompStore::new();
	let writer = voter_lib::ModComp::new(3,&mut compStore,sl ,thd_send_same);
	let mut channel = Channel::new(
		writer,
		voter_lib::ModComp::new(1,&mut compStore,sl ,no_op),
		&mut compStore,
		sl
	);
	wait_for_done(&(compStore.components[writer]),sl);
	println!("Validate Msgs: Expected: True : Actual: {:?},", channel.lock().deref().validate_msgs(0));
}

fn thd_send_same(sl:Sl, rep:  Arc<Lock<Replica>>) {
	println!("Running {:?}",rep.lock().deref());
	while rep.lock().deref().channel.is_none() {
		sl.block_for(Duration::new(1,0));
	}

	let mut rep = rep.lock();
	let rep_id = rep.deref().rep_id;
	let unit_of_work = rep.deref().unit_of_work;

	let msg = String::from("Hello").into_bytes();
	println!("Rep {:?} Send msg {:?}", rep_id,msg);
	rep.deref_mut().channel.as_mut().expect("rep has no chan").lock().deref_mut().send(msg,rep_id,unit_of_work);
	rep.state_transition(Done);
}

// /* -------------------- Test Channel msg faultfind ------------------------ */

pub fn test_chan_fault_find(sl:Sl) {
	test_print("Begin channel msg Fault find test");
	let mut compStore = voter_lib::CompStore::new();
	let writer = voter_lib::ModComp::new(3,&mut compStore,sl ,thd_send_diff);
	let mut channel = Channel::new(
		writer,
		voter_lib::ModComp::new(1,&mut compStore,sl ,no_op),
		&mut compStore,
		sl
	);
	wait_for_done(&(compStore.components[writer]),sl);
	println!("Validate Msgs: Expected: False : Actual: {:?},", channel.lock().deref().validate_msgs(0));
	let faulted = channel.lock().deref().find_fault(0);
	println!("Chan Find Fault: Expected: 2 : Actual: {:?}", faulted);
	channel.lock().deref_mut().poison(faulted as u16);
	println!("Channel poison: Expected no messages from rep 2 : actual: {:?}", channel.lock().deref().messages);
}

fn thd_send_diff(sl:Sl, rep:  Arc<Lock<Replica>>) {
	println!("Running {:?}",rep.lock().deref());
	while rep.lock().deref().channel.is_none() {
		sl.block_for(Duration::new(1,0));
	}

	let mut rep = rep.lock();
	let rep_id = rep.deref().rep_id;
	let unit_of_work = rep.deref().unit_of_work;

	let msg = if rep_id > 1 {String::from("Hello").into_bytes()} else {String::from("World").into_bytes()};
	println!("Rep {:?} Send msg {:?}", rep_id,msg);
	rep.deref_mut().channel.as_mut().expect("rep has no chan").lock().deref_mut().send(msg,rep_id,unit_of_work);
	rep.state_transition(Done);
}


// /* -------------- Test lib Composite chagnes ------------------- */

// pub fn test_lib_composite(sl:Sl) {
// 	let mut thd = sl.spawn(move |sl:Sl| {println!("thd started")});
// 	thd.set_param(ThreadParameter::Priority(5));
// 	println!("Thread id {}", thd.thdid());
// 	println!("Current Thread {}", sl.current_thread().thdid());
// }


fn no_op(sl:Sl, rep:  Arc<Lock<Replica>>) {}

