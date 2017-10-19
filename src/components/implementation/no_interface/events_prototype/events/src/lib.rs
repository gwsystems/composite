use std::time::Duration;

extern crate lib_composite;
use lib_composite::kernel_api::DefKernelAPI;
use lib_composite::sl::{ThreadParameter, Sl};

mod channel;
use channel::{Channel, ChannelHandle};


#[no_mangle]
pub extern fn rust_init() {
    println!("This a print from Rust!");

    let api = unsafe {
        DefKernelAPI::assert_already_initialized()
    };

    Sl::start_scheduler_loop_without_initializing(api, 30, move |sl: Sl| {
        println!("Entered scheduler loop");
        let channel: &Channel<u64> = &Channel::new(sl);
        println!("Got handle");


        let mut receiver_handle = ChannelHandle::new(channel);
        let mut receiver = sl.spawn(move |_| {
            println!("Entered receiver thread");
            loop {
                println!("Preparing to receive");
                let data = receiver_handle.receive();
                println!("Received {}", data);
            }
        });
        receiver.set_param(ThreadParameter::Priority(5));


        let mut sender_handle = ChannelHandle::new(channel);
        let mut sender = sl.spawn(move |sl: Sl| {
            println!("Entered sender thread");

            let mut acc = 0;
            loop {
                sender_handle.send(acc);
                println!("Sent {}", acc);
                sl.block_for(Duration::from_secs(acc * 5));
                acc += 1;
            }
        });
        sender.set_param(ThreadParameter::Priority(10));

        println!("Exiting main thread");
    });
}
