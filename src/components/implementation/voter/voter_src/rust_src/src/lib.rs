mod voter;
mod unit_tests;
mod demo_app;

#[macro_use] extern crate lib_composite;
#[macro_use] extern crate lazy_static;

use lib_composite::kernel_api::DefKernelAPI;
use lib_composite::sl::Sl;
use lib_composite::panic_trace;

#[no_mangle]
pub extern fn test_call_rs() {
	printc!("Executing test call in Rust");
}

#[no_mangle]
pub extern fn rust_init() {
	let api = unsafe {
		DefKernelAPI::assert_already_initialized()
	};
	Sl::start_scheduler_loop_without_initializing(api, 30, move |sl: Sl| {
		// panic_trace::trace_init();
		println!("Entered Sched loop\n=========================");
		demo_app::start(sl);
	});
}


fn _run_tests(_sl:Sl) {
	// unit_tests::test_state_logic(sl,2);
	// unit_tests::test_wakeup(sl,2);
	// unit_tests::test_vote_simple(sl,2);
	// //unit_tests::test_channel_create(sl,1);
	// unit_tests::test_snd_rcv(sl,1);
	// unit_tests::test_chan_validate(sl);
	// unit_tests::test_chan_fault_find(sl);
	// unit_tests::test_store(sl);
}

