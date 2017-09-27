mod voter_lib;
mod unit_tests;

#[macro_use] extern crate lib_composite;
use lib_composite::kernel_api::DefKernelAPI;
use lib_composite::sl::{ThreadParameter, Sl};

	
#[no_mangle]
pub extern fn rust_init() {
	println!("Entering Rust ---------------\n");

	let api = unsafe {
		DefKernelAPI::assert_already_initialized()
	};

	Sl::start_scheduler_loop_without_initializing(api, 30, move |sl: Sl| {
		println!("Entered Sched loop");
		//unit_tests::test_wakeup(sl,1);
		//unit_tests::test_vote_simple(sl);
		unit_tests::test_lib_composite(sl);
	});
}

