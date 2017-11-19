#![allow(dead_code)] /* DEBUG REMOVE THIS */
use std::fmt;
use lib_composite::sl_lock::Lock;
use lib_composite::sl::Sl;
use std::sync::Arc;
use std::ops::{DerefMut,Deref};
use voter_lib::*;
use voter_lib::MAX_REPS;

pub struct Channel  {
	pub reader_id:  usize,
	pub writer_id:  usize,
	messages: Vec<ChannelData>,
}

pub struct ChannelData {
	pub msg_id: u16,
	pub rep_id: u16,
	pub message: Box<[u8]>,
}


impl fmt::Debug for Channel {
	fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {write!(f, "Reader_id: {} | Writer_id: {}", self.reader_id, self.writer_id)}
}

impl ChannelData {
	pub fn compare_msg_to(&self, other_data:&ChannelData) -> bool {
		let ref msg = other_data.message;
		if msg.len() != self.message.len() {return false}

		for i in 0..msg.len() {
			if msg[i] != self.message[i] {return false}
		}

		return true;
	}
}

impl Channel {
	pub fn new(reader_id:usize, writer_id:usize, compStore:&mut CompStore, sl:Sl) -> Arc<Lock<Channel>> {
		let chan = Arc::new(Lock::new(sl,
			Channel {
				reader_id,
				writer_id,
				messages:Vec::new(),
			}
		));

		let ref mut components = compStore.components;

		for i in 0..components[reader_id].num_replicas {
			components[reader_id].replicas[i].lock().deref_mut().channel = Some(Arc::clone(&chan));
		}

		for i in 0..components[writer_id].num_replicas {
			components[writer_id].replicas[i].lock().deref_mut().channel = Some(Arc::clone(&chan));
		}

		return Arc::clone(&chan);
	}

	pub fn call_vote(&self,comp_store:&mut CompStore) -> (VoteStatus,VoteStatus) {
		//check to make sure messages on the channel are valid data
		let unit_of_work = comp_store.components[self.writer_id].replicas[0].lock().deref().unit_of_work;
		if (!self.validate_msgs(unit_of_work)) {
			//if not find the replica with invalid messages
			let faulted = self.find_fault(unit_of_work);
			assert!(faulted > 0);
			//remove these messages from the chanel
			//self.poison(faulted)
			//return a faild vote
			return (VoteStatus::Fail(self.writer_id,faulted as u16),
					comp_store.components[self.reader_id].collect_vote())
		}

		return (comp_store.components[self.writer_id].collect_vote(),
				comp_store.components[self.reader_id].collect_vote())
	}

	pub fn send(&mut self, msg:Vec<u8>, rep_id:u16,msg_id:u16) {
		self.messages.push(
			ChannelData {
				msg_id,
				rep_id,
				message: msg.into_boxed_slice(),
			}
		)
	}

	pub fn receive(&mut self) -> Option<ChannelData> {
		self.messages.pop()
	}

	pub fn has_data(&self) -> bool {
		return !self.messages.is_empty()
	}

	pub fn validate_msgs(&self,msg_id:u16) -> bool {
		if self.messages.len() == 0 {return true}

		//outter loop find a message with the passed in id to compare to
		for msg in &self.messages {
			if msg.msg_id != msg_id {continue}
			//compare all other messages with this id against msg
			for msg_b in &self.messages {
				if msg_b.msg_id != msg_id {continue}
				//if the msgs dont match return and well handle finding the fault elsewhere
				if !msg.compare_msg_to(&msg_b) {return false}
			}

			break;
		}

		return true;
	}

	pub fn find_fault(&self,msg_id:u16) -> i16 {
		//store the number of replicas that agree, and rep id of sender
		let mut concensus: [(u8,i16); MAX_REPS] = [(0,0); MAX_REPS];

		//find which replica disagrees with the majority
		let mut i = 0;
		for msg in &self.messages {
			if msg.msg_id != msg_id {continue} /* skip messages that have been validated but not read */
			concensus[i].1 = msg.rep_id as i16;
			for msg_b in &self.messages {
				if msg_b.msg_id != msg_id {continue}
				//if the msgs agree mark that
				if (msg.compare_msg_to(&msg_b)) {
					concensus[i].0 += 1;
				}
			}

			i+=1;
		}

		//go through concensus to get the rep id that sent the msg with least agreement
		let mut min = 4;
		let mut faulted = -1;
		for val in concensus.iter() {
			if val.0 < min {
				min = val.0;
				faulted = val.1;
			}
		}
		return faulted;
	}

	pub fn wake_all(&mut self,comp_store:& mut CompStore) {
		comp_store.components[self.reader_id].wake_all();
		comp_store.components[self.writer_id].wake_all();
	}
}