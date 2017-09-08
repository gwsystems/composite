Rust with Composite, a How To
=============================

First time installation
-----------------------
1. Install rustup, following the instructions [here](https://rustup.rs/).
2. Setup your path correctly by running `source $HOME/.cargo/env` (this should
    only be necessary for your current shell, any new shell should have the path
    setup automatically)  
3. Set your toolchain to nightly by running `rustup default nightly`
4. Install `xargo` for cross compiling by running `cargo install xargo`
5. Install `rust-src` so `xargo` can cross compile it for us by running `rustup component add rust-src`
6. You're done! `make` should just work!

Updating to a newer Rust / xargo
--------------------------------
1. Ensure you have OpenSSL installed with `sudo apt-get install pkg-config libssl-dev`
2. Ensure you have cargo-update installed with `cargo install cargo-update`
3. Update rust with `rustup update`
4. Update xargo with `cargo install-update -a`

Do you have a Rust related bug?
-------------------------------
Message @peachg on the composite slack, or @Others on Github (that's a real username)
