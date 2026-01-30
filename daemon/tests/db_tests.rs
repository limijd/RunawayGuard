use runaway_daemon::db::Database;
use tempfile::tempdir;

#[test]
fn test_create_database() {
    let dir = tempdir().unwrap();
    let db_path = dir.path().join("test.db");
    let db = Database::open(&db_path).unwrap();
    db.init_schema().unwrap();
    assert!(db_path.exists());
}

#[test]
fn test_insert_and_query_alert() {
    let dir = tempdir().unwrap();
    let db_path = dir.path().join("test.db");
    let db = Database::open(&db_path).unwrap();
    db.init_schema().unwrap();
    db.insert_alert(1234, "test_process", "/usr/bin/test", "cpu_high", "warning").unwrap();
    let alerts = db.get_alerts(10, None).unwrap();
    assert_eq!(alerts.len(), 1);
    assert_eq!(alerts[0].pid, 1234);
    assert_eq!(alerts[0].name, "test_process");
}

#[test]
fn test_whitelist_operations() {
    let dir = tempdir().unwrap();
    let db_path = dir.path().join("test.db");
    let db = Database::open(&db_path).unwrap();
    db.init_schema().unwrap();
    db.add_whitelist("chrome", "name", Some("Browser")).unwrap();
    assert!(db.is_whitelisted("chrome", "name").unwrap());
    assert!(!db.is_whitelisted("firefox", "name").unwrap());
}
