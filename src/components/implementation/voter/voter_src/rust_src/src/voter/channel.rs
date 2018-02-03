#![allow(dead_code)] /* DEBUG REMOVE THIS */
use std::fmt;
use lib_composite::sl_lock::Lock;
use lib_composite::sl::Sl;
use std::sync::Arc;
use std::ops::{DerefMut,Deref};
use voter::voter_lib::*;
use voter::voter_lib::MAX_REPS;
use voter::*;

pub struct Channel  {
	pub reader_id:  Option<usize>,
	pub writer_id:  Option<usize>,
	pub messages: Vec<ChannelData>,
}

pub struct ChannelData {
	pub message: Box<[u8]>,
}

impl fmt::Debug for Channel {
	fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {write!(f, "Reader_id: {} | Writer_id: {}", self.reader_id.unwrap_or(999), self.writer_id.unwrap_or(999))}
}

impl fmt::Debug for ChannelData {
	fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {write!(f, "message {:?}\n", self.message)}
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

	pub fn join(chan_lock:&mut Arc<Lock<Channel>>, comp_id:usize, is_reader:bool) -> bool {
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

		let compStore(ref component) = COMPONENTS[comp_id];
		let ref mut component = component.lock();

		for i in 0..component.deref().as_ref().unwrap().num_replicas {
			let ref mut rep_lock = component.deref_mut().as_mut().unwrap().replicas[i];
			rep_lock.channel = Some(Arc::clone(&chan_lock));
		}

		return true
	}

	pub fn call_vote(&mut self) -> Result<(VoteStatus,VoteStatus), String> {
		let reader_id = if self.reader_id.is_some() {self.reader_id.unwrap()} else {return Err("call_vote fail, no readerid on chan".to_string())};
		let writer_id = if self.reader_id.is_some() {self.writer_id.unwrap()} else {return Err("call_vote fail, no writerid on chan".to_string())};

		let compStore(ref comp_store_wrapper) = COMPONENTS[reader_id];
		let ref mut reader_lock = comp_store_wrapper.lock();
		if reader_lock.deref().is_none() {return Err(format!("call_vote fail, no reader at {}",reader_id))}
		let mut reader = reader_lock.deref_mut().as_mut().unwrap();

		let compStore(ref comp_store_wrapper) = COMPONENTS[writer_id];
		let ref mut writer_lock = comp_store_wrapper.lock();
		if writer_lock.deref().is_none() {return Err(format!("call_vote fail, no writer at {}",writer_id))}
		let mut writer = writer_lock.deref_mut().as_mut().unwrap();

		let reader_vote = reader.collect_vote();

		let mut writer_vote = writer.collect_vote();
		if writer_vote == VoteStatus::Success {
			if !writer.validate_msgs() {
				let faulted = writer.find_faulted_msg();
				assert!(faulted>-1);
				writer_vote = VoteStatus::Fail(faulted as u16);
			}
			else if writer.new_data {
				self.transfer(writer);
			}
		}

		Ok((writer_vote,reader_vote))
	}

	pub fn send(&mut self, msg:Vec<u8>) {
		self.messages.push(
			ChannelData {
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

	//transfer validated data from replica local buffers to a channel message
	fn transfer(&mut self,writer:&mut voter_lib::ModComp) {
		{
			self.send(writer.replicas[0].write_buffer.to_vec()); //TODO update channel data strucutre to use static allocations
		}
		//clear write_buffers
		for i in 0..writer.num_replicas {
			for j in 0..voter_lib::WRITE_BUFF_SIZE {
				writer.replicas[i].write_buffer[j] = 0;
			}
		}
		writer.new_data = false;
	}

	pub fn wake_all(&mut self, comp_store:&[compStore; MAX_COMPS]) -> bool {
		let reader_id = if self.reader_id.is_some() {self.reader_id.unwrap()} else {return false};
		let writer_id = if self.reader_id.is_some() {self.writer_id.unwrap()} else {return false};

		let compStore(ref reader) = comp_store[reader_id];
		let ref mut reader = reader.lock();
		reader.deref_mut().as_mut().unwrap().wake_all();

		let compStore(ref writer) = comp_store[writer_id];
		let ref mut writer = writer.lock();
		writer.deref_mut().as_mut().unwrap().wake_all();

		return true;
	}
}