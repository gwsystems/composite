mod voter;
mod unit_tests;
mod demo_app;

#[macro_use] extern crate lib_composite;
#[macro_use] extern crate lazy_static;

use lib_composite::kernel_api::DefKernelAPI;
use lib_composite::sl::Sl;
use lib_composite::sys::sl;
use lib_composite::panic_trace;

extern {
    pub fn sl_thd_curr_rs() -> *mut sl::sl_thd;
    fn print_hack(n: i8);
}

#[no_mangle]
pub extern fn test_call_rs() {
	unsafe {print_hack(2)};
	printc!("Executing test call in Rust");
}

#[no_mangle]
pub extern fn interface_handeler(op: u8) {
	unsafe {print_hack(1)};
	match op {
		0 => Sl::spawn2(move |sl: Sl| {test_call_rs();}),
		_ => panic!("Invalid interface operateion: {}",op),
	};


	// unsafe {
	// 	let thd_ptr = sl_thd_curr_rs();
	// 	assign_thread_data(thd_ptr);
	// }
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

fn run_tests(sl:Sl) {
	// unit_tests::test_state_logic(sl,2);
	// unit_tests::test_wakeup(sl,2);
	// unit_tests::test_vote_simple(sl,2);
	// //unit_tests::test_channel_create(sl,1);
	// unit_tests::test_snd_rcv(sl,1);
	// unit_tests::test_chan_validate(sl);
	// unit_tests::test_chan_fault_find(sl);
	// unit_tests::test_store(sl);
}

