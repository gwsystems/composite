use voter::voter_lib::ReplicaState::*;
use voter::voter_lib::Replica;
use lib_composite::sl::Sl;
use lib_composite::sl_lock::Lock;
use std::sync::Arc;
use std::ops::{DerefMut,Deref};
use std::time::Duration;
use voter::channel::*;
use voter::voter_app_init;

pub fn start(sl:Sl) {
	//initialize replicas.
	voter_app_init(3,sl,do_work);
	//create a fake serviec provider
	//conntect replicas to facke service provider
	//create channels
}

fn do_work(sl:Sl, rep: Arc<Lock<Replica>>) {
	return
}

fn make_systemcall(sys_call:i8) {
	return
}