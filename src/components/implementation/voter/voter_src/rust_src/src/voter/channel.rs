#![allow(dead_code)] /* DEBUG REMOVE THIS */
use std::fmt;
use lib_composite::sl_lock::Lock;
use lib_composite::sl::Sl;
use std::sync::Arc;
use std::ops::{DerefMut,Deref};
use voter::voter_lib::*;
use voter::voter_lib::MAX_REPS;

pub struct Channel  {
	pub reader_id:  Option<usize>,
	pub writer_id:  Option<usize>,
	pub messages: Vec<ChannelData>,
}

pub struct ChannelData {
	pub msg_id: u16,
	pub rep_id: u16,
	pub message: Box<[u8]>,
}


impl fmt::Debug for Channel {
	fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {write!(f, "Reader_id: {} | Writer_id: {}", self.reader_id.unwrap_or(999), self.writer_id.unwrap_or(999))}
}

impl fmt::Debug for ChannelData {
	fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {write!(f, "Msg id: {} | rep id: {} | message {:?}\n", self.msg_id, self.rep_id, self.message)}
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
	pub fn new(sl:Sl) -> Arc<Lock<Channel>> {
		let chan = Arc::new(Lock::new(sl,
			Channel {
				reader_id:None,
				writer_id:None,
				messages:Vec::new(),
			}
		));

		return Arc::clone(&chan);
	}

	pub fn join(chan_lock:Arc<Lock<Channel>>, comp_id:usize, comp_store:&mut CompStore, is_reader:bool) -> bool {
		let ref mut chan = chan_lock.lock();
		match is_reader {
			true  => {
				if chan.deref().reader_id.is_some() {return false}
				chan.deref_mut().reader_id = Some(comp_id)
			},
			false => {
				if chan.deref().writer_id.is_some() {return false}
				chan.deref_mut().writer_id = Some(comp_id)
			},
		};

		let ref mut components = comp_store.components;

		for i in 0..components[comp_id].num_replicas {
			components[comp_id].replicas[i].lock().deref_mut().channel = Some(Arc::clone(&chan_lock));
		}

		return true
	}

	pub fn call_vote(&mut self,comp_store:&mut CompStore) -> Result<(VoteStatus,VoteStatus), &'static str> {
		let reader_id = if self.reader_id.is_some() {self.reader_id.unwrap()} else {return Err("call_vote fail, no reader")};
		let writer_id = if self.reader_id.is_some() {self.writer_id.unwrap()} else {return Err("call_vote fail, no writer")};

		//check to make sure messages on the channel are valid data
		let unit_of_work = comp_store.components[writer_id].replicas[0].lock().deref().unit_of_work;
		if !self.validate_msgs(unit_of_work) {
			//if not find the replica with invalid messages
			let faulted = self.find_fault(unit_of_work);
			assert!(faulted > 0);
			//remove these messages from the chanel
			self.poison(faulted as u16);
			//return a faild vote
			return Ok((VoteStatus::Fail(writer_id,faulted as u16),
					comp_store.components[reader_id].collect_vote()))
		}

		Ok((comp_store.components[writer_id].collect_vote(),
		    comp_store.components[reader_id].collect_vote()))
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

	pub fn find_fault(&self, msg_id:u16) -> i16 {
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
				if msg.compare_msg_to(&msg_b) {
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

	pub fn poison(&mut self, rep_id:u16) {
		self.messages.retain(|ref msg| msg.rep_id != rep_id);
	}

	pub fn wake_all(&mut self, comp_store:& mut CompStore) -> bool {
		let reader_id = if self.reader_id.is_some() {self.reader_id.unwrap()} else {return false};
		let writer_id = if self.reader_id.is_some() {self.writer_id.unwrap()} else {return false};

		comp_store.components[reader_id].wake_all();
		comp_store.components[writer_id].wake_all();

		return true;
	}
}