pub mod voter_lib;
pub mod channel;

use lib_composite::sl::Sl;
use lib_composite::sl_lock::Lock;
use std::sync::Arc;
use std::ops::{DerefMut,Deref};
use std::time::Duration;
use lazy_static;

lazy_static! {
	static ref COMPONENTS:Lock<voter_lib::CompStore> = unsafe {Lock::new(Sl::assert_scheduler_already_started(), voter_lib::CompStore::new())};
	static ref CHANNELS:Lock<Vec<Arc<Lock<channel::Channel>>>>  = unsafe {Lock::new(Sl::assert_scheduler_already_started(), Vec::new())};
}

pub fn voter_app_init(num_replicas: usize, sl:Sl,thd_entry: fn(sl:Sl, rep: Arc<Lock<voter_lib::Replica>>)) -> usize {
	voter_lib::ModComp::new(num_replicas,&mut COMPONENTS.lock().deref_mut(),sl,thd_entry)
}

pub fn create_channel(sl:Sl) -> usize {
 return 0;
}

pub fn join_channel_reader(chan_id: usize) {

}

pub fn join_channel_writer(chan_id: usize) {

}