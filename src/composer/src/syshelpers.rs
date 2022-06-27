use pipers::Pipe;
use std::fs;

// FIXME: progs should be a more general iteration type
// return a tuple of stdout/stderr
pub fn exec_pipeline(progs: Vec<String>) -> (String, String) {
    let output = progs.iter()
        .fold(None,
              |upstream: Option<Pipe>, cmd| match upstream {
                  None => Some(Pipe::new(cmd)),  // initial command
                  Some(up) => Some(up.then(cmd)) // piped commands
              })
        .unwrap_or_else(|| Pipe::new("cat /dev/null"))
        .finally()
        .expect("Failure in constructing pipe.")
        .wait_with_output()
        .expect("Failure in executing pipe.");
    (String::from_utf8(output.stdout).unwrap(), String::from_utf8(output.stderr).unwrap())
}

pub fn dump_file(name: &String) -> Result<Vec<u8>, String> {
    use std::fs::File;
    use std::io::Read;

    let file = File::open(name);
    if let Err(e) = file {
        return Err(String::from(format!("{}: {}", &name, e.to_string())));
    }

    let mut buf = Vec::new();
    if file.unwrap().read_to_end(&mut buf).is_err() {
        return Err(String::from(format!("Could not read data out of file {}.", &name)))
    } else {
        Ok(buf)
    }
}

pub fn emit_file(name: &String, output: &[u8]) -> Result<(), String> {
    if let Err(_) = fs::write(name, &output) {
        return Err(String::from(format!("Could not write to file {}.\n", name)));
    }

    let len = output.len();
    match fs::metadata(name) {
        Ok(md) => if md.len() as usize != len {
            Err(String::from(format!("File {} written to, but not correct length.\n", name)))
        } else {
            Ok(())
        },
        _ => Err(String::from(format!("Could not retrieve the metadata for file {}.\n", name)))
    }
}

// remove directory, all contents, and remake it
pub fn reset_dir(dirname: &String) -> Result<(), String> {
    assert!(dirname != "/");    // small sanity check
    let _ = fs::remove_dir_all(&dirname); // failure here is fine; we're creating next anyway
    match fs::create_dir(&dirname) {
        Ok(_) => Ok(()),
        Err(_) => Err(String::from(format!("Could not create directory {}\n", dirname)))
    }
}

pub fn dir_exists(dirname: &String) -> bool {
    fs::read_dir(&dirname).is_ok()
}
