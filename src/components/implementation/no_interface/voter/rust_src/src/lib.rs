mod voter_lib;
extern crate lib_composite;


	
#[no_mangle]
pub extern fn rust_init() {
  println!("Entering Rust ---------------\n");
  let comp = voter_lib::ModComp::new(3);
  println!("replica is {:?}",comp);
}

