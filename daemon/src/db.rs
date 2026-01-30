//! SQLite database operations

use rusqlite::Connection;
use std::path::Path;

pub struct Database {
    conn: Connection,
}

impl Database {
    pub fn open(path: &Path) -> rusqlite::Result<Self> {
        let conn = Connection::open(path)?;
        Ok(Self { conn })
    }

    pub fn init_schema(&self) -> rusqlite::Result<()> {
        self.conn.execute_batch(include_str!("../schema.sql"))
    }
}
