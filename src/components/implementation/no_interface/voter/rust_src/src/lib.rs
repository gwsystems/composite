#[macro_use] mod voter_lib;
mod unit_tests;

#[macro_use] extern crate lib_composite;
use lib_composite::kernel_api::DefKernelAPI;
use lib_composite::sl::{ThreadParameter, Sl};


#[no_mangle]
pub extern fn rust_init() {
	printc!("Entering Rust ---------------\n");
	printc!("test outside\n");

	let api = unsafe {
		DefKernelAPI::assert_already_initialized()
	};

	Sl::start_scheduler_loop_without_initializing(api, 30, move |sl: Sl| {
		printc!("test inside\n");
		// printc!("Entered Sched loop");
		//unit_tests::test_wakeup(sl,3);
		//unit_tests::test_vote_simple(sl);
	});
}

