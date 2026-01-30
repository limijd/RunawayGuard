//! SQLite database operations

use rusqlite::{Connection, params};
use std::path::Path;
use std::time::{SystemTime, UNIX_EPOCH};

pub struct Database {
    conn: Connection,
}

#[derive(Debug, Clone)]
pub struct AlertRecord {
    pub id: i64,
    pub timestamp: i64,
    pub pid: u32,
    pub name: String,
    pub cmdline: String,
    pub reason: String,
    pub severity: String,
    pub resolved: bool,
    pub action_taken: Option<String>,
}

#[derive(Debug, Clone)]
pub struct WhitelistEntry {
    pub id: i64,
    pub pattern: String,
    pub match_type: String,
    pub reason: Option<String>,
}

impl Database {
    pub fn open(path: &Path) -> rusqlite::Result<Self> {
        if let Some(parent) = path.parent() {
            std::fs::create_dir_all(parent).ok();
        }
        let conn = Connection::open(path)?;
        Ok(Self { conn })
    }

    pub fn init_schema(&self) -> rusqlite::Result<()> {
        self.conn.execute_batch(include_str!("../schema.sql"))
    }

    fn now() -> i64 {
        SystemTime::now().duration_since(UNIX_EPOCH).map(|d| d.as_secs() as i64).unwrap_or(0)
    }

    pub fn insert_alert(&self, pid: u32, name: &str, cmdline: &str, reason: &str, severity: &str) -> rusqlite::Result<i64> {
        self.conn.execute(
            "INSERT INTO alerts (timestamp, pid, name, cmdline, reason, severity) VALUES (?1, ?2, ?3, ?4, ?5, ?6)",
            params![Self::now(), pid, name, cmdline, reason, severity],
        )?;
        Ok(self.conn.last_insert_rowid())
    }

    pub fn get_alerts(&self, limit: u32, since: Option<i64>) -> rusqlite::Result<Vec<AlertRecord>> {
        let mut stmt = if since.is_some() {
            self.conn.prepare(
                "SELECT id, timestamp, pid, name, cmdline, reason, severity, resolved, action_taken
                 FROM alerts WHERE timestamp >= ?1 ORDER BY timestamp DESC LIMIT ?2"
            )?
        } else {
            self.conn.prepare(
                "SELECT id, timestamp, pid, name, cmdline, reason, severity, resolved, action_taken
                 FROM alerts ORDER BY timestamp DESC LIMIT ?1"
            )?
        };

        let rows = if let Some(since_ts) = since {
            stmt.query_map(params![since_ts, limit], Self::map_alert)?
        } else {
            stmt.query_map(params![limit], Self::map_alert)?
        };
        rows.collect()
    }

    fn map_alert(row: &rusqlite::Row) -> rusqlite::Result<AlertRecord> {
        Ok(AlertRecord {
            id: row.get(0)?,
            timestamp: row.get(1)?,
            pid: row.get(2)?,
            name: row.get(3)?,
            cmdline: row.get(4)?,
            reason: row.get(5)?,
            severity: row.get(6)?,
            resolved: row.get::<_, i32>(7)? != 0,
            action_taken: row.get(8)?,
        })
    }

    pub fn resolve_alert(&self, id: i64, action: &str) -> rusqlite::Result<()> {
        self.conn.execute("UPDATE alerts SET resolved = 1, action_taken = ?1 WHERE id = ?2", params![action, id])?;
        Ok(())
    }

    pub fn add_whitelist(&self, pattern: &str, match_type: &str, reason: Option<&str>) -> rusqlite::Result<i64> {
        self.conn.execute(
            "INSERT INTO whitelist (pattern, match_type, reason, created_at) VALUES (?1, ?2, ?3, ?4)",
            params![pattern, match_type, reason, Self::now()],
        )?;
        Ok(self.conn.last_insert_rowid())
    }

    pub fn remove_whitelist(&self, id: i64) -> rusqlite::Result<()> {
        self.conn.execute("DELETE FROM whitelist WHERE id = ?1", params![id])?;
        Ok(())
    }

    pub fn get_whitelist(&self) -> rusqlite::Result<Vec<WhitelistEntry>> {
        let mut stmt = self.conn.prepare("SELECT id, pattern, match_type, reason FROM whitelist")?;
        let rows = stmt.query_map([], |row| {
            Ok(WhitelistEntry {
                id: row.get(0)?,
                pattern: row.get(1)?,
                match_type: row.get(2)?,
                reason: row.get(3)?,
            })
        })?;
        rows.collect()
    }

    pub fn is_whitelisted(&self, pattern: &str, match_type: &str) -> rusqlite::Result<bool> {
        let count: i32 = self.conn.query_row(
            "SELECT COUNT(*) FROM whitelist WHERE pattern = ?1 AND match_type = ?2",
            params![pattern, match_type],
            |row| row.get(0),
        )?;
        Ok(count > 0)
    }

    pub fn insert_sample(&self, name: &str, pid: u32, cpu_percent: f64, memory_mb: f64, runtime_seconds: u64) -> rusqlite::Result<()> {
        self.conn.execute(
            "INSERT INTO process_samples (timestamp, name, pid, cpu_percent, memory_mb, runtime_seconds) VALUES (?1, ?2, ?3, ?4, ?5, ?6)",
            params![Self::now(), name, pid, cpu_percent, memory_mb, runtime_seconds as i64],
        )?;
        Ok(())
    }

    pub fn cleanup_old_data(&self, samples_days: u32, alerts_days: u32) -> rusqlite::Result<()> {
        let samples_cutoff = Self::now() - (samples_days as i64 * 86400);
        let alerts_cutoff = Self::now() - (alerts_days as i64 * 86400);
        self.conn.execute("DELETE FROM process_samples WHERE timestamp < ?1", params![samples_cutoff])?;
        self.conn.execute("DELETE FROM alerts WHERE timestamp < ?1 AND resolved = 1", params![alerts_cutoff])?;
        Ok(())
    }
}
