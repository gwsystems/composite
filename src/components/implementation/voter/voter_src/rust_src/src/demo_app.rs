use voter::voter_lib::ReplicaState::*;
use voter::voter_lib::Replica;
use lib_composite::sl::Sl;
use lib_composite::sl_lock::Lock;
use std::sync::Arc;
use std::ops::{DerefMut,Deref};
use std::time::Duration;
use voter::channel::*;
use voter;

pub fn start(sl:Sl) {
	println!("Test app initializing");
	//initialize replicas.
	let comp_id = voter::voter_new_app_init(3,sl,do_work);
	//create a fake serviec provider
	//conntect replicas to facke service provider
	//create channels
	let chan_id = voter::channel_create(sl);
	println!("created comp {:?} created chan {:?}", comp_id,chan_id);
}

fn do_work(sl:Sl, rep: Arc<Lock<Replica>>) {
	return
}

fn make_systemcall(sys_call:i8) {
	return
}