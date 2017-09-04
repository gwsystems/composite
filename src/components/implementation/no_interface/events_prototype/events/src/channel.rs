use std::collections::VecDeque;
use std::sync::Arc;

use lib_composite::sl::{Sl, Thread};
use lib_composite::sl_lock::Lock;

pub struct Channel<EventType> {
    data: Arc<Lock<ChannelData<EventType>>>
}

impl <EventType> Channel<EventType> {
    pub fn new(sl: Sl) -> Channel<EventType> {
        Channel {
            data: Arc::new(Lock::new(sl, ChannelData {
                sl: Sl,
                buffer: VecDeque::new(),
                waiters: VecDeque::new()
            }))
        }
    }
}

struct ChannelData<EventType> {
    sl: Sl,
    buffer: VecDeque<EventType>,
    // TODO: Consider an ordered set here
    waiters: VecDeque<Thread>
}

#[allow(dead_code)]
impl <EventType> ChannelData<EventType> {
    fn send(&mut self, event: EventType) {
        self.buffer.push_back(event);
        let client = self.waiters.pop_front();
        match client {
            Some(mut thread) => thread.wakeup(),
            None => {}
        }
    }

    fn try_receive(&mut self) -> Option<EventType> {
        self.buffer.pop_front()
    }

    fn receive_or_become_waiter(&mut self) -> Option<EventType> {
        let event = self.buffer.pop_front();

        if event.is_none() {
            let thread = self.sl.current_thread();

            if !self.waiters.contains(&thread) {
                self.waiters.push_back(thread);
            }
        }

        event
    }

    fn abandon_waiting(&mut self) {
        let thread = self.sl.current_thread();

        // Retain all waiters that are not this thread
        self.waiters.retain(|t| t != &thread);
    }
}

#[derive(Clone)]
pub struct ChannelHandle<EventType> {
    sl: Sl,
    channel: Arc<Lock<ChannelData<EventType>>>
}

#[allow(dead_code)]
impl<EventType> ChannelHandle<EventType> {
    pub fn new(channel: &Channel<EventType>) -> Self {
        ChannelHandle {
            // We know if a channel exists, the scheduler has been started
            sl: Sl::assert_scheduler_already_started(),
            channel: channel.data.clone()
        }
    }


    pub fn send(&mut self, event: EventType) {
        let mut channel = self.channel.lock();
        channel.send(event);
    }

    pub fn try_receive(&mut self) -> Option<EventType> {
        self.channel.lock().try_receive()
    }

    pub fn receive(&mut self) -> EventType {
        loop {
            let optional_event = {
                self.channel.lock().receive_or_become_waiter()
            };

            if let Some(event) = optional_event {
                return event;
            }

            // This may seem racy, since we might be woken up before we block (since we release the
            // channel lock.) However sl does the right thing here, and fixes this race for us.
            self.sl.block();
        }
    }
}


pub struct MultiChannelHandle<EventType> {
    sl: Sl,
    channels: Vec<Arc<Lock<ChannelData<EventType>>>>,
    // We rotate through which channel we send to, so we need to keep a count
    count: usize
}

#[allow(dead_code)]
impl<EventType> MultiChannelHandle<EventType> {
    pub fn new(c: &[&Channel<EventType>]) -> Self {
        if c.len() == 0 {
            panic!("Cannot create MultiChannelHandle from 0 channels!")
        }

        let mut channels = Vec::new();
        for &channel in c {
            channels.push(channel.data.clone());
        }

        MultiChannelHandle {
            sl: Sl::assert_scheduler_already_started(),
            channels: channels,
            count: 0
        }
    }

    pub fn send(&mut self, event: EventType) {
        // TODO: Figure out if this is the right "fair" way to do this
        let chan = &self.channels[self.count % self.channels.len()];
        chan.lock().send(event);
        self.count += 1;
    }

    fn poll_channels_and_become_waiter(&mut self) -> Option<EventType> {
        for channel in &self.channels {
            if let Some(event) = channel.lock().receive_or_become_waiter() {
                return Some(event)
            }
        }
        None
    }

    pub fn receive(&mut self) -> EventType {
        // We loop until we get some value
        let event  = loop {
            if let Some(event) = self.poll_channels_and_become_waiter() {
                break event;
            }

            self.sl.block();
        };

        // Then we stop waiting on anything
        for channel in &self.channels {
            channel.lock().abandon_waiting()
        }

        // FIXME: There is a race condition here. Consider:
        // - Thd 1) Awoken to receive an event
        // - Thd 2) Sends an event, wakes up Thd 1 to recieve it
        // - Thd 3) Sends an event, tries to wake up Thd 1
        // Thd 3 will crash since it is trying to wake up an awoken thread

        // One workaround is to let already awoken threads be awoken again in Sl. Then add a case at
        // the end of the function (after abandoning waiting)

        event
    }
}
