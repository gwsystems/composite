use std::process::{Child, Command, Output, Stdio};

pub struct Pipe {
    cur: Child,
}

impl Pipe {
    pub fn new(cmd: &str) -> Self {
        let args: Vec<&str> = cmd.split(' ').collect();

        Self {
            cur: Command::new(args[0])
                .args(&args[1..])
                .stdout(Stdio::piped())
                .spawn()
                .expect("Failed to run command"),
        }
    }

    pub fn output(self) -> Option<Output> {
        self.cur.wait_with_output().ok()
    }

    pub fn next(self, next: &str) -> Self {
        let args: Vec<&str> = next.split(' ').collect();
        let new_cmd = Command::new(args[0])
            .args(&args[1..])
            .stdin(self.cur.stdout.unwrap()) // It's spawned, so it's ok to unwrap
            .stdout(Stdio::piped())
            .spawn()
            .expect("Failed to run command");

        Self { cur: new_cmd }
    }
}
