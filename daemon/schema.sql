-- Process samples for learning
CREATE TABLE IF NOT EXISTS process_samples (
    id INTEGER PRIMARY KEY,
    timestamp INTEGER NOT NULL,
    name TEXT NOT NULL,
    pid INTEGER NOT NULL,
    cpu_percent REAL,
    memory_mb REAL,
    runtime_seconds INTEGER
);
CREATE INDEX IF NOT EXISTS idx_samples_name_time ON process_samples(name, timestamp);

-- Alert records
CREATE TABLE IF NOT EXISTS alerts (
    id INTEGER PRIMARY KEY,
    timestamp INTEGER NOT NULL,
    pid INTEGER NOT NULL,
    name TEXT NOT NULL,
    cmdline TEXT,
    reason TEXT NOT NULL,
    severity TEXT NOT NULL,
    resolved INTEGER DEFAULT 0,
    action_taken TEXT
);
CREATE INDEX IF NOT EXISTS idx_alerts_time ON alerts(timestamp DESC);

-- Process statistics for learning
CREATE TABLE IF NOT EXISTS process_stats (
    name TEXT PRIMARY KEY,
    sample_count INTEGER,
    cpu_avg REAL,
    cpu_p95 REAL,
    memory_avg REAL,
    memory_p95 REAL,
    last_updated INTEGER
);

-- Whitelist
CREATE TABLE IF NOT EXISTS whitelist (
    id INTEGER PRIMARY KEY,
    pattern TEXT NOT NULL,
    match_type TEXT NOT NULL,
    reason TEXT,
    created_at INTEGER
);
