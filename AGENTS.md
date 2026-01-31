# RunawayGuard Agent Notes

## Environment

### Rust Toolchain
- Use Rust/Cargo version 1.89
- Executables have `-1.89` suffix: `rustc-1.89`, `cargo-1.89`
- Example: `cargo-1.89 build --release`

### Qt/GUI Build
- Build directory: `gui/build`
- If CMakeCache.txt has stale paths, delete `gui/build` and reconfigure:
  ```bash
  cd gui && rm -rf build && mkdir build && cd build && cmake .. && make -j$(nproc)
  ```

## Debugging Tips

### GUI Stuck at "Starting daemon..."
**Symptom**: Status bar shows "Starting daemon..." indefinitely.

**Root cause**: Stale socket file at `/run/user/<uid>/runaway-guard.sock` from a previous daemon run, but daemon process is not running.

**Quick fix**: Remove the stale socket:
```bash
rm -f /run/user/$(id -u)/runaway-guard.sock
```

**Permanent fix**: Commit `b92d8b0` added proper stale socket detection in `DaemonManager::tryConnect()`.

## Architecture Notes

### GUI-Daemon IPC
- Unix domain socket at `/run/user/<uid>/runaway-guard.sock`
- JSON-line protocol
- `DaemonManager` handles lifecycle (start, crash recovery, reconnect)
- `DaemonClient` handles socket communication
