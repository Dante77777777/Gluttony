from __future__ import annotations

import hashlib
import sqlite3
import unittest
import uuid
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MIGRATION_PATH = ROOT / "database" / "migrations" / "001_initial_schema.sql"
NOW_MS = 1_784_736_000_000


class SqliteSchemaTests(unittest.TestCase):
    def setUp(self) -> None:
        self.assertGreaterEqual(
            sqlite3.sqlite_version_info,
            (3, 37, 0),
            "Schema version 1 requires SQLite 3.37+ for STRICT tables.",
        )

        self.connection = sqlite3.connect(":memory:")
        self.connection.row_factory = sqlite3.Row
        self.connection.execute("PRAGMA foreign_keys = ON")

        migration_sql = MIGRATION_PATH.read_text(encoding="utf-8")
        self.connection.executescript(migration_sql)

        checksum = hashlib.sha256(migration_sql.encode("utf-8")).hexdigest()
        self.connection.execute(
            """
            INSERT INTO schema_migration (
                version,
                name,
                checksum,
                applied_at_ms
            )
            VALUES (?, ?, ?, ?)
            """,
            (1, MIGRATION_PATH.name, checksum, NOW_MS),
        )
        self.connection.execute("PRAGMA user_version = 1")
        self.connection.commit()

    def tearDown(self) -> None:
        self.connection.close()

    def insert_running_run(
        self,
        *,
        run_id: str | None = None,
        total_count: int = 1,
        storage_origin: str = "MANAGED",
    ) -> str:
        run_id = run_id or str(uuid.uuid4())
        self.connection.execute(
            """
            INSERT INTO inspection_run (
                run_id,
                status,
                source_type,
                source_path,
                storage_path,
                storage_origin,
                model_path,
                model_format,
                model_hash,
                app_version,
                review_threshold,
                ng_threshold,
                heatmap_alpha,
                started_at_ms,
                updated_at_ms,
                total_count
            )
            VALUES (
                :run_id,
                'RUNNING',
                'FOLDER',
                :source_path,
                :storage_path,
                :storage_origin,
                :model_path,
                'ONNX',
                :model_hash,
                '1.0.0',
                0.45,
                0.60,
                0.55,
                :now_ms,
                :now_ms,
                :total_count
            )
            """,
            {
                "run_id": run_id,
                "source_path": "D:/images",
                "storage_path": f"D:/gluttony/runs/{run_id}",
                "storage_origin": storage_origin,
                "model_path": "D:/models/model.onnx",
                "model_hash": "a" * 64,
                "now_ms": NOW_MS,
                "total_count": total_count,
            },
        )
        return run_id

    def insert_pending_item(
        self,
        run_id: str,
        *,
        result_id: str | None = None,
        sequence_no: int = 1,
    ) -> str:
        result_id = result_id or str(uuid.uuid4())
        self.connection.execute(
            """
            INSERT INTO inspection_item (
                result_id,
                run_id,
                sequence_no,
                source_file_name,
                source_file_path,
                source_file_size,
                source_modified_at_ms,
                status,
                created_at_ms,
                updated_at_ms
            )
            VALUES (?, ?, ?, ?, ?, ?, ?, 'PENDING', ?, ?)
            """,
            (
                result_id,
                run_id,
                sequence_no,
                f"{sequence_no:03}.png",
                f"D:/images/{sequence_no:03}.png",
                1024,
                NOW_MS,
                NOW_MS,
                NOW_MS,
            ),
        )
        return result_id

    def claim_item(self, result_id: str, *, at_ms: int = NOW_MS + 1) -> None:
        cursor = self.connection.execute(
            """
            UPDATE inspection_item
            SET
                status = 'PROCESSING',
                attempt_count = attempt_count + 1,
                processing_started_at_ms = ?,
                updated_at_ms = ?
            WHERE result_id = ?
              AND status = 'PENDING'
            """,
            (at_ms, at_ms, result_id),
        )
        self.assertEqual(cursor.rowcount, 1)

    def complete_success(
        self,
        result_id: str,
        *,
        level: str = "NG",
        score: float = 0.75,
        at_ms: int = NOW_MS + 2,
    ) -> None:
        cursor = self.connection.execute(
            """
            UPDATE inspection_item
            SET
                status = 'SUCCESS',
                detection_level = ?,
                score = ?,
                inference_ms = 42,
                defect_area = 100,
                heatmap_rel_path = ?,
                completed_at_ms = ?,
                updated_at_ms = ?
            WHERE result_id = ?
              AND status = 'PROCESSING'
            """,
            (
                level,
                score,
                f"heatmaps/{result_id}.png",
                at_ms,
                at_ms,
                result_id,
            ),
        )
        self.assertEqual(cursor.rowcount, 1)

    def finish_run_as_draft(
        self,
        run_id: str,
        *,
        ok_count: int = 0,
        review_count: int = 0,
        ng_count: int = 1,
        failed_count: int = 0,
        score_sum: float = 0.75,
        at_ms: int = NOW_MS + 3,
    ) -> None:
        completed_count = ok_count + review_count + ng_count + failed_count
        cursor = self.connection.execute(
            """
            UPDATE inspection_run
            SET
                status = 'DRAFT',
                completed_count = ?,
                failed_count = ?,
                ok_count = ?,
                review_count = ?,
                ng_count = ?,
                score_sum = ?,
                completed_at_ms = ?,
                updated_at_ms = ?,
                revision = revision + 1
            WHERE run_id = ?
              AND status = 'RUNNING'
            """,
            (
                completed_count,
                failed_count,
                ok_count,
                review_count,
                ng_count,
                score_sum,
                at_ms,
                at_ms,
                run_id,
            ),
        )
        self.assertEqual(cursor.rowcount, 1)

    def save_run(self, run_id: str, *, at_ms: int = NOW_MS + 4) -> None:
        cursor = self.connection.execute(
            """
            UPDATE inspection_run
            SET
                status = 'SAVED',
                name = 'Bottle inspection',
                note = 'Schema integration test',
                saved_at_ms = ?,
                manifest_version = 1,
                manifest_rel_path = 'batch.json',
                manifest_sha256 = ?,
                items_rel_path = 'items.jsonl',
                items_sha256 = ?,
                updated_at_ms = ?,
                revision = revision + 1
            WHERE run_id = ?
              AND status = 'DRAFT'
            """,
            (at_ms, "b" * 64, "c" * 64, at_ms, run_id),
        )
        self.assertEqual(cursor.rowcount, 1)

    def test_schema_objects_and_migration_metadata_exist(self) -> None:
        tables = {
            row["name"]
            for row in self.connection.execute(
                "SELECT name FROM sqlite_master WHERE type = 'table'"
            )
        }
        self.assertTrue(
            {
                "schema_migration",
                "inspection_run",
                "inspection_item",
                "storage_cleanup_job",
            }.issubset(tables)
        )

        triggers = {
            row["name"]
            for row in self.connection.execute(
                "SELECT name FROM sqlite_master WHERE type = 'trigger'"
            )
        }
        self.assertEqual(len(triggers), 7)

        indexes = {
            row["name"]
            for row in self.connection.execute(
                """
                SELECT name
                FROM sqlite_master
                WHERE type = 'index'
                  AND name NOT LIKE 'sqlite_autoindex%'
                """
            )
        }
        self.assertEqual(len(indexes), 5)

        views = {
            row["name"]
            for row in self.connection.execute(
                "SELECT name FROM sqlite_master WHERE type = 'view'"
            )
        }
        self.assertEqual(
            views,
            {
                "v_inspection_run_history",
                "v_inspection_run_reconciliation",
            },
        )

        migration = self.connection.execute(
            "SELECT version, name, length(checksum) AS checksum_length "
            "FROM schema_migration"
        ).fetchone()
        self.assertEqual(migration["version"], 1)
        self.assertEqual(migration["name"], MIGRATION_PATH.name)
        self.assertEqual(migration["checksum_length"], 64)
        self.assertEqual(
            self.connection.execute("PRAGMA user_version").fetchone()[0],
            1,
        )

    def test_run_threshold_and_saved_name_constraints(self) -> None:
        run_id = str(uuid.uuid4())
        with self.assertRaises(sqlite3.IntegrityError):
            self.connection.execute(
                """
                INSERT INTO inspection_run (
                    run_id,
                    status,
                    source_type,
                    source_path,
                    storage_path,
                    storage_origin,
                    model_path,
                    model_format,
                    model_hash,
                    app_version,
                    review_threshold,
                    ng_threshold,
                    heatmap_alpha,
                    started_at_ms,
                    updated_at_ms
                )
                VALUES (
                    ?, 'RUNNING', 'FOLDER', 'D:/images', 'D:/runs/x',
                    'MANAGED', 'D:/model.onnx', 'ONNX', ?, '1.0.0',
                    0.70, 0.60, 0.55, ?, ?
                )
                """,
                (run_id, "a" * 64, NOW_MS, NOW_MS),
            )

        run_id = self.insert_running_run()
        result_id = self.insert_pending_item(run_id)
        self.claim_item(result_id)
        self.complete_success(result_id)
        self.finish_run_as_draft(run_id)

        with self.assertRaises(sqlite3.IntegrityError):
            self.connection.execute(
                """
                UPDATE inspection_run
                SET
                    status = 'SAVED',
                    saved_at_ms = ?,
                    manifest_version = 1,
                    manifest_rel_path = 'batch.json',
                    manifest_sha256 = ?,
                    items_rel_path = 'items.jsonl',
                    items_sha256 = ?,
                    updated_at_ms = ?
                WHERE run_id = ?
                """,
                (NOW_MS + 4, "b" * 64, "c" * 64, NOW_MS + 4, run_id),
            )

        with self.assertRaises(sqlite3.IntegrityError):
            self.connection.execute(
                """
                UPDATE inspection_run
                SET
                    status = 'SAVED',
                    name = 'Missing manifest hash',
                    saved_at_ms = ?,
                    manifest_version = 1,
                    manifest_rel_path = 'batch.json',
                    items_rel_path = 'items.jsonl',
                    items_sha256 = ?,
                    updated_at_ms = ?
                WHERE run_id = ?
                """,
                (NOW_MS + 4, "c" * 64, NOW_MS + 4, run_id),
            )

    def test_foreign_key_unique_sequence_and_cascade(self) -> None:
        missing_run_id = str(uuid.uuid4())
        with self.assertRaises(sqlite3.IntegrityError):
            self.insert_pending_item(missing_run_id)

        run_id = self.insert_running_run()
        self.insert_pending_item(run_id, sequence_no=1)
        with self.assertRaises(sqlite3.IntegrityError):
            self.insert_pending_item(run_id, sequence_no=1)

        self.connection.execute(
            "DELETE FROM inspection_run WHERE run_id = ?",
            (run_id,),
        )
        remaining = self.connection.execute(
            "SELECT COUNT(*) FROM inspection_item WHERE run_id = ?",
            (run_id,),
        ).fetchone()[0]
        self.assertEqual(remaining, 0)

    def test_batch_and_input_creation_roll_back_together(self) -> None:
        run_id = str(uuid.uuid4())

        with self.assertRaises(sqlite3.IntegrityError):
            with self.connection:
                self.insert_running_run(run_id=run_id, total_count=2)
                self.insert_pending_item(run_id, sequence_no=1)
                self.insert_pending_item(run_id, sequence_no=1)

        run_count = self.connection.execute(
            "SELECT COUNT(*) FROM inspection_run WHERE run_id = ?",
            (run_id,),
        ).fetchone()[0]
        item_count = self.connection.execute(
            "SELECT COUNT(*) FROM inspection_item WHERE run_id = ?",
            (run_id,),
        ).fetchone()[0]
        self.assertEqual(run_count, 0)
        self.assertEqual(item_count, 0)

    def test_item_state_machine_and_result_shape_constraints(self) -> None:
        run_id = self.insert_running_run()
        result_id = self.insert_pending_item(run_id)

        with self.assertRaises(sqlite3.IntegrityError):
            self.connection.execute(
                "UPDATE inspection_item SET status = 'SUCCESS' "
                "WHERE result_id = ?",
                (result_id,),
            )

        self.claim_item(result_id)

        with self.assertRaises(sqlite3.IntegrityError):
            self.connection.execute(
                """
                UPDATE inspection_item
                SET
                    status = 'SUCCESS',
                    detection_level = 'NG',
                    score = 0.75,
                    inference_ms = 42,
                    defect_area = 100,
                    completed_at_ms = ?,
                    updated_at_ms = ?
                WHERE result_id = ?
                """,
                (NOW_MS + 2, NOW_MS + 2, result_id),
            )

        with self.assertRaises(sqlite3.IntegrityError):
            self.connection.execute(
                """
                UPDATE inspection_item
                SET
                    status = 'FAILED',
                    completed_at_ms = ?,
                    updated_at_ms = ?
                WHERE result_id = ?
                """,
                (NOW_MS + 2, NOW_MS + 2, result_id),
            )

        self.complete_success(result_id)
        row = self.connection.execute(
            "SELECT status, detection_level, score FROM inspection_item "
            "WHERE result_id = ?",
            (result_id,),
        ).fetchone()
        self.assertEqual(row["status"], "SUCCESS")
        self.assertEqual(row["detection_level"], "NG")
        self.assertAlmostEqual(row["score"], 0.75)

    def test_run_cannot_finish_with_pending_items(self) -> None:
        run_id = self.insert_running_run()
        self.insert_pending_item(run_id)

        with self.assertRaises(sqlite3.IntegrityError):
            self.finish_run_as_draft(run_id)

        status = self.connection.execute(
            "SELECT status FROM inspection_run WHERE run_id = ?",
            (run_id,),
        ).fetchone()[0]
        self.assertEqual(status, "RUNNING")

    def test_complete_save_history_and_reconciliation(self) -> None:
        run_id = self.insert_running_run()
        result_id = self.insert_pending_item(run_id)
        self.claim_item(result_id)
        self.complete_success(result_id, level="NG", score=0.75)
        self.finish_run_as_draft(run_id, ng_count=1, score_sum=0.75)
        self.save_run(run_id)

        history = self.connection.execute(
            "SELECT * FROM v_inspection_run_history WHERE run_id = ?",
            (run_id,),
        ).fetchone()
        self.assertEqual(history["name"], "Bottle inspection")
        self.assertEqual(history["ng_count"], 1)
        self.assertAlmostEqual(history["average_score"], 0.75)

        reconciliation = self.connection.execute(
            "SELECT * FROM v_inspection_run_reconciliation WHERE run_id = ?",
            (run_id,),
        ).fetchone()
        self.assertEqual(
            reconciliation["stored_total"],
            reconciliation["actual_total"],
        )
        self.assertEqual(
            reconciliation["stored_completed"],
            reconciliation["actual_completed"],
        )
        self.assertEqual(
            reconciliation["stored_ng"],
            reconciliation["actual_ng"],
        )

    def test_crash_recovery_resets_processing_item(self) -> None:
        run_id = self.insert_running_run()
        result_id = self.insert_pending_item(run_id)
        self.claim_item(result_id)

        with self.connection:
            self.connection.execute(
                """
                UPDATE inspection_item
                SET
                    status = 'PENDING',
                    processing_started_at_ms = NULL,
                    updated_at_ms = ?
                WHERE result_id = ?
                  AND status = 'PROCESSING'
                """,
                (NOW_MS + 2, result_id),
            )
            self.connection.execute(
                """
                UPDATE inspection_run
                SET
                    status = 'INTERRUPTED',
                    last_interrupted_at_ms = ?,
                    updated_at_ms = ?,
                    revision = revision + 1
                WHERE run_id = ?
                  AND status = 'RUNNING'
                """,
                (NOW_MS + 2, NOW_MS + 2, run_id),
            )

        item = self.connection.execute(
            """
            SELECT status, attempt_count, processing_started_at_ms
            FROM inspection_item
            WHERE result_id = ?
            """,
            (result_id,),
        ).fetchone()
        self.assertEqual(item["status"], "PENDING")
        self.assertEqual(item["attempt_count"], 1)
        self.assertIsNone(item["processing_started_at_ms"])

        cursor = self.connection.execute(
            """
            UPDATE inspection_run
            SET
                status = 'RUNNING',
                updated_at_ms = ?,
                revision = revision + 1
            WHERE run_id = ?
              AND status = 'INTERRUPTED'
            """,
            (NOW_MS + 3, run_id),
        )
        self.assertEqual(cursor.rowcount, 1)

    def test_terminal_items_and_saved_runs_are_immutable(self) -> None:
        run_id = self.insert_running_run()
        result_id = self.insert_pending_item(run_id)
        self.claim_item(result_id)
        self.complete_success(result_id)
        self.finish_run_as_draft(run_id)
        self.save_run(run_id)

        with self.assertRaises(sqlite3.IntegrityError):
            self.connection.execute(
                "UPDATE inspection_item SET score = 0.9 WHERE result_id = ?",
                (result_id,),
            )

        with self.assertRaises(sqlite3.IntegrityError):
            self.connection.execute(
                "UPDATE inspection_run SET name = 'Changed' WHERE run_id = ?",
                (run_id,),
            )

    def test_cleanup_job_path_is_unique(self) -> None:
        target_path = "D:/gluttony/runs/obsolete"
        self.connection.execute(
            """
            INSERT INTO storage_cleanup_job (
                target_path,
                target_kind,
                reason,
                created_at_ms
            )
            VALUES (?, 'DIRECTORY', 'DISCARD_DRAFT', ?)
            """,
            (target_path, NOW_MS),
        )

        with self.assertRaises(sqlite3.IntegrityError):
            self.connection.execute(
                """
                INSERT INTO storage_cleanup_job (
                    target_path,
                    target_kind,
                    reason,
                    created_at_ms
                )
                VALUES (?, 'DIRECTORY', 'DELETE_SAVED', ?)
                """,
                (target_path, NOW_MS),
            )


if __name__ == "__main__":
    unittest.main()
