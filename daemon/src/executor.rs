//! Process action executor (kill, nice, etc.)

use std::process::Command;

#[derive(Debug, Clone, Copy)]
pub enum Signal {
    Term,
    Kill,
    Stop,
    Cont,
}

pub fn send_signal(pid: u32, signal: Signal) -> std::io::Result<()> {
    let sig = match signal {
        Signal::Term => "TERM",
        Signal::Kill => "KILL",
        Signal::Stop => "STOP",
        Signal::Cont => "CONT",
    };
    Command::new("kill")
        .args(["-s", sig, &pid.to_string()])
        .status()?;
    Ok(())
}

pub fn renice(pid: u32, priority: i32) -> std::io::Result<()> {
    Command::new("renice")
        .args(["-n", &priority.to_string(), "-p", &pid.to_string()])
        .status()?;
    Ok(())
}
