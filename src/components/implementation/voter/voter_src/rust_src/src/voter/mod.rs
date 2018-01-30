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

pub fn voter_new_app_init(num_replicas: usize, sl:Sl,thd_entry: fn(sl:Sl, rep_id: usize)) -> usize {
	let mut comp_free_idx = COMP_FREE.lock();
	let comp_id = *comp_free_idx.deref();

	let compStore(ref comp) = COMPONENTS[comp_id];
	let ref mut comp = comp.lock();
	comp.deref_mut().get_or_insert(voter_lib::ModComp::new(num_replicas,comp_id,sl,thd_entry));

	*comp_free_idx.deref_mut() += 1;
	return comp_id;
}

pub fn channel_create(sl:Sl) -> usize {
	let mut chan_free_idx = CHAN_FREE.lock();
	let chan_id = *chan_free_idx.deref();

	let channelStore(ref channel) = CHANNELS[chan_id];
	let ref mut channel = channel.lock();
	channel.deref_mut().get_or_insert(channel::Channel::new(sl));

	*chan_free_idx.deref_mut() += 1;
	return chan_id;
}

pub fn channel_join_reader(chan_id:usize, comp_id:usize) {
	let is_reader = true;
	let channelStore(ref chan) = CHANNELS[chan_id];
	channel::Channel::join(chan.lock().deref_mut().as_mut().unwrap(),comp_id,is_reader);
}

pub fn channel_join_writer(chan_id:usize, comp_id:usize) {
	let is_reader = false;
	let channelStore(ref chan) = CHANNELS[chan_id];
	channel::Channel::join(chan.lock().deref_mut().as_mut().unwrap(),comp_id,is_reader);
}

pub fn channel_snd(data:Vec<u8>, chan_id:usize, comp_id:usize, rep_id: usize, sl:Sl) {
	let channelStore(ref chan_store_wrapper_lock) = CHANNELS[chan_id];
	let ref mut chan_store_wrapper = chan_store_wrapper_lock.lock();
	let ref mut chan_lock = chan_store_wrapper.deref_mut().as_mut().unwrap();

	match voter_lib::ModComp::replica_communicate(comp_id, rep_id, chan_lock.lock().deref_mut(), voter_lib::ReplicaState::Written, sl) {
		Ok(ret_val) => println!("rep ret val {}",ret_val.unwrap_or(99999)),
		Err(e) => {
			println!("{}",e);
			return
		},
	}

	//fixme - msg id should be set with UOW (which we need to calculate in cheduling)
	chan_lock.lock().deref_mut().send(data,rep_id as u16,0);
	sl.block();
}

pub fn channel_rcv(chan_id:usize, comp_id:usize, rep_id: usize, sl:Sl) -> Option<channel::ChannelData> {
	let channelStore(ref chan_store_wrapper_lock) = CHANNELS[chan_id];
	let ref mut chan_store_wrapper = chan_store_wrapper_lock.lock();
	let ref mut chan_lock = chan_store_wrapper.deref_mut().as_mut().unwrap();

	match voter_lib::ModComp::replica_communicate(comp_id, rep_id, chan_lock.lock().deref_mut(), voter_lib::ReplicaState::Read, sl) {
		Ok(ret_val) => println!("rep ret val {}",ret_val.unwrap_or(99999)),
		Err(e) => {
			println!("{}",e);
			return None
		},
	}

	let mut chan = chan_lock.lock();
	chan.deref_mut().receive()
}

pub fn channel_wake(chan_id:usize) {
	let channelStore(ref chan_store_wrapper_lock) = CHANNELS[chan_id];
	let ref mut chan_store_wrapper = chan_store_wrapper_lock.lock();
	let ref mut chan_lock = chan_store_wrapper.deref_mut().as_mut().unwrap();

	chan_lock.lock().deref_mut().wake_all(&COMPONENTS);
}

pub fn replica_processing(comp_id:usize, rep_id:usize) {
	let compStore(ref comp_store_wrapper_lock) = COMPONENTS[comp_id];
	let mut comp = comp_store_wrapper_lock.lock();
	let comp = comp.deref_mut().as_mut().unwrap();

	let mut rep = comp.replicas[rep_id].lock();
	rep.deref_mut().state_transition(voter_lib::ReplicaState::Processing);
}