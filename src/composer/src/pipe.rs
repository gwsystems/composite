use std::process::{Child, Command, Output, Stdio};
extern crate shell_words;

pub struct Pipe {
    cur: Child,
}

impl Pipe {
    pub fn new(cmd: &str) -> Self {
        let args = shell_words::split(cmd).expect("Failed to parse command in pipe fn init");
        let command = &args[0];
        let args: Vec<&str> = args.iter().map(|s| s.as_str()).collect();

        Self {
            cur: Command::new(command)
                .args(&args[1..])
                .stdout(Stdio::piped())
                .spawn()
                .expect(&format!("Failed to run command: \"{}\"", command)),
        }
    }

    pub fn output(self) -> Option<Output> {
        self.cur.wait_with_output().ok()
    }

    pub fn next(self, next: &str) -> Self {
        let args = shell_words::split(next).expect("Failed to parse command in pipe fn next");
        let command = &args[0];
        let args: Vec<&str> = args.iter().map(|s| s.as_str()).collect();

        let new_cmd = Command::new(command)
            .args(&args[1..])
            .stdin(self.cur.stdout.unwrap()) // It's spawned, so it's ok to unwrap
            .stdout(Stdio::piped())
            .spawn()
            .expect(&format!("Failed to run command: \"{}\"", command));

        Self { cur: new_cmd }
    }
}
