pub mod voter_lib;
pub mod channel;

use lib_composite::sl::Sl;
use lib_composite::sl_lock::Lock;
use std::sync::Arc;
use std::ops::{DerefMut,Deref};
use std::time::Duration;
use lazy_static;

const MAX_CHANNELS:usize = 10;
const MAX_COMPS:usize = 5;

//if we store channel id on replica i think we can remove the inner arc/lock
pub struct channelStore(pub Lock<Option<Arc<Lock<channel::Channel>>>>);
impl Default for channelStore {
	fn default() -> channelStore {
		let sl = unsafe {Sl::assert_scheduler_already_started()};
		channelStore(Lock::new(sl, None))
	}
}

pub struct compStore(pub Lock<Option<voter_lib::ModComp>>);
impl Default for compStore {
	fn default() -> compStore {
		let sl = unsafe {Sl::assert_scheduler_already_started()};
		compStore(Lock::new(sl, None))
	}
}

lazy_static! {
	pub static ref COMPONENTS:[compStore; MAX_COMPS] = Default::default();
	pub static ref COMP_FREE:Lock<usize> = unsafe {Lock::new(Sl::assert_scheduler_already_started(),0)};

	pub static ref CHANNELS:[channelStore; MAX_CHANNELS] = Default::default();
	pub static ref CHAN_FREE:Lock<usize> = unsafe {Lock::new(Sl::assert_scheduler_already_started(),0)};
}

pub fn voter_component_sched_loop(sl:Sl) {
	//todo - this function will iterate through all of the components in the voter, dispatch their replicas and trigger their votes at the
	//end of a cycle.
}

pub fn voter_app_init(num_replicas: usize, sl:Sl,thd_entry: fn(sl:Sl, rep: Arc<Lock<voter_lib::Replica>>)) -> usize {
	voter_lib::ModComp::new(num_replicas,0,sl,thd_entry);
	return 0 //todo
}

pub fn channel_create(sl:Sl) -> usize {
 return 0;
}

pub fn channel_join_reader(chan_id:usize) {

}

pub fn channel_join_writer(chan_id:usize) {

}

pub fn channel_snd(data:Vec<u8>, chan_id:usize, comp_id:usize, rep_id: usize, sl:Sl) {
	// let mut components_lock = COMPONENTS.lock();
	// let mut channels_lock = CHANNELS.lock();

	// let ref mut comp = components_lock.deref_mut().components[comp_id];
	// let ref chan_lock = channels_lock.deref()[chan_id];

	// match comp.replica_communicate(sl, rep_id, chan_lock.lock().deref_mut(),components_lock.deref_mut(), voter_lib::ReplicaState::Written) {
	// 	Ok(ret_val) => println!("rep ret val {}",ret_val.unwrap_or(99999)),
	// 	Err(e) => {
	// 		println!("{}",e);
	// 		return
	// 	},
	// }

	// let ref mut chan = chan_lock.lock();
	// //fixme - msg id should be set with UOW
	// chan.deref_mut().send(data,rep_id as u16,0);
}

// pub fn channel_rcv(chan_id:usize, chan_id:usize, rep_id: usize) -> Vec<u8> {

// }