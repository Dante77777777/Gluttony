-- Add first-class TCP camera runs while preserving all version 1 data.

DROP VIEW v_inspection_run_reconciliation;

-- gluttony:statement-break

DROP VIEW v_inspection_run_history;

-- gluttony:statement-break

CREATE TABLE inspection_run_v2 (
    run_id                  TEXT PRIMARY KEY,
    name                    TEXT,
    note                    TEXT,
    status                  TEXT NOT NULL,

    source_type             TEXT NOT NULL,
    source_path             TEXT NOT NULL,
    peer_address            TEXT,

    storage_path            TEXT NOT NULL,
    storage_origin          TEXT NOT NULL,

    model_path              TEXT NOT NULL,
    model_format            TEXT NOT NULL,
    model_hash              TEXT NOT NULL,
    app_version             TEXT NOT NULL,

    review_threshold        REAL NOT NULL,
    ng_threshold            REAL NOT NULL,
    heatmap_alpha           REAL NOT NULL,

    started_at_ms           INTEGER NOT NULL,
    completed_at_ms         INTEGER,
    saved_at_ms             INTEGER,
    last_interrupted_at_ms  INTEGER,
    updated_at_ms           INTEGER NOT NULL,

    total_count             INTEGER NOT NULL DEFAULT 0,
    completed_count         INTEGER NOT NULL DEFAULT 0,
    failed_count            INTEGER NOT NULL DEFAULT 0,
    ok_count                INTEGER NOT NULL DEFAULT 0,
    review_count            INTEGER NOT NULL DEFAULT 0,
    ng_count                INTEGER NOT NULL DEFAULT 0,
    score_sum               REAL NOT NULL DEFAULT 0,
    received_count          INTEGER NOT NULL DEFAULT 0,
    dropped_count           INTEGER NOT NULL DEFAULT 0,

    manifest_version        INTEGER,
    manifest_rel_path       TEXT,
    manifest_sha256         TEXT,
    items_rel_path          TEXT,
    items_sha256            TEXT,

    revision                INTEGER NOT NULL DEFAULT 0,

    CHECK (length(run_id) = 36),
    CHECK (name IS NULL OR length(trim(name)) BETWEEN 1 AND 120),
    CHECK (note IS NULL OR length(note) <= 4000),
    CHECK (status IN ('RUNNING', 'DRAFT', 'SAVED', 'INTERRUPTED')),
    CHECK (source_type IN ('SINGLE_IMAGE', 'FOLDER', 'TCP_CAMERA')),
    CHECK (length(trim(source_path)) > 0),
    CHECK (
        (source_type = 'TCP_CAMERA'
         AND peer_address IS NOT NULL
         AND length(trim(peer_address)) > 0)
        OR
        (source_type <> 'TCP_CAMERA' AND peer_address IS NULL)
    ),
    CHECK (length(trim(storage_path)) > 0),
    CHECK (storage_origin IN ('MANAGED', 'EXTERNAL')),
    CHECK (length(trim(model_path)) > 0),
    CHECK (model_format IN ('ONNX', 'OPENVINO')),
    CHECK (length(model_hash) = 64),
    CHECK (length(trim(app_version)) > 0),

    CHECK (review_threshold >= 0.0 AND review_threshold <= 1.0),
    CHECK (ng_threshold >= 0.0 AND ng_threshold <= 1.0),
    CHECK (review_threshold < ng_threshold),
    CHECK (heatmap_alpha >= 0.0 AND heatmap_alpha <= 1.0),

    CHECK (started_at_ms > 0),
    CHECK (updated_at_ms >= started_at_ms),
    CHECK (completed_at_ms IS NULL OR completed_at_ms >= started_at_ms),
    CHECK (saved_at_ms IS NULL OR saved_at_ms >= started_at_ms),
    CHECK (
        last_interrupted_at_ms IS NULL
        OR last_interrupted_at_ms >= started_at_ms
    ),

    CHECK (total_count >= 0),
    CHECK (completed_count >= 0 AND completed_count <= total_count),
    CHECK (failed_count >= 0),
    CHECK (ok_count >= 0),
    CHECK (review_count >= 0),
    CHECK (ng_count >= 0),
    CHECK (received_count >= 0),
    CHECK (dropped_count >= 0 AND dropped_count <= received_count),
    CHECK (
        completed_count =
        failed_count + ok_count + review_count + ng_count
    ),

    CHECK (
        status NOT IN ('DRAFT', 'SAVED')
        OR (
            total_count > 0
            AND completed_count = total_count
            AND completed_at_ms IS NOT NULL
        )
    ),

    CHECK (
        status <> 'SAVED'
        OR (
            name IS NOT NULL
            AND length(trim(name)) BETWEEN 1 AND 120
        )
    ),

    CHECK (
        (
            status = 'SAVED'
            AND saved_at_ms IS NOT NULL
            AND manifest_version IS NOT NULL
            AND manifest_version > 0
            AND manifest_rel_path IS NOT NULL
            AND length(trim(manifest_rel_path)) > 0
            AND manifest_sha256 IS NOT NULL
            AND length(manifest_sha256) = 64
            AND items_rel_path IS NOT NULL
            AND length(trim(items_rel_path)) > 0
            AND items_sha256 IS NOT NULL
            AND length(items_sha256) = 64
        )
        OR
        (
            status <> 'SAVED'
            AND saved_at_ms IS NULL
            AND manifest_version IS NULL
            AND manifest_rel_path IS NULL
            AND manifest_sha256 IS NULL
            AND items_rel_path IS NULL
            AND items_sha256 IS NULL
        )
    ),

    CHECK (revision >= 0)
) STRICT;

-- gluttony:statement-break

CREATE TABLE inspection_item_v2 (
    result_id                  TEXT PRIMARY KEY,
    run_id                     TEXT NOT NULL,
    sequence_no                INTEGER NOT NULL,

    source_file_name           TEXT NOT NULL,
    source_file_path           TEXT NOT NULL,
    source_file_size           INTEGER NOT NULL,
    source_modified_at_ms      INTEGER NOT NULL,
    source_file_hash           TEXT,

    transport_sequence         TEXT,
    captured_at_ms             INTEGER,
    pixel_format               INTEGER,
    image_width                INTEGER,
    image_height               INTEGER,

    status                     TEXT NOT NULL,
    detection_level            TEXT,
    score                      REAL,
    inference_ms               INTEGER,
    defect_area                INTEGER,

    heatmap_rel_path           TEXT,
    thumbnail_rel_path         TEXT,
    error_message              TEXT,

    attempt_count              INTEGER NOT NULL DEFAULT 0,
    processing_started_at_ms   INTEGER,
    completed_at_ms            INTEGER,
    created_at_ms              INTEGER NOT NULL,
    updated_at_ms              INTEGER NOT NULL,

    FOREIGN KEY (run_id)
        REFERENCES inspection_run_v2(run_id)
        ON UPDATE RESTRICT
        ON DELETE CASCADE,

    UNIQUE (run_id, sequence_no),

    CHECK (length(result_id) = 36),
    CHECK (length(run_id) = 36),
    CHECK (sequence_no >= 1),
    CHECK (length(trim(source_file_name)) > 0),
    CHECK (length(trim(source_file_path)) > 0),
    CHECK (source_file_size >= 0),
    CHECK (source_modified_at_ms >= 0),
    CHECK (
        source_file_hash IS NULL
        OR length(source_file_hash) = 64
    ),
    CHECK (
        transport_sequence IS NULL
        OR length(trim(transport_sequence)) > 0
    ),
    CHECK (captured_at_ms IS NULL OR captured_at_ms >= 0),
    CHECK (pixel_format IS NULL OR pixel_format >= 0),
    CHECK (image_width IS NULL OR image_width > 0),
    CHECK (image_height IS NULL OR image_height > 0),

    CHECK (status IN ('PENDING', 'PROCESSING', 'SUCCESS', 'FAILED')),
    CHECK (
        detection_level IS NULL
        OR detection_level IN ('OK', 'REVIEW', 'NG')
    ),
    CHECK (inference_ms IS NULL OR inference_ms >= 0),
    CHECK (defect_area IS NULL OR defect_area >= 0),
    CHECK (attempt_count >= 0),
    CHECK (created_at_ms > 0),
    CHECK (updated_at_ms >= created_at_ms),

    CHECK (
        status <> 'PENDING'
        OR (
            detection_level IS NULL
            AND score IS NULL
            AND inference_ms IS NULL
            AND defect_area IS NULL
            AND heatmap_rel_path IS NULL
            AND thumbnail_rel_path IS NULL
            AND error_message IS NULL
            AND processing_started_at_ms IS NULL
            AND completed_at_ms IS NULL
        )
    ),

    CHECK (
        status <> 'PROCESSING'
        OR (
            attempt_count >= 1
            AND processing_started_at_ms IS NOT NULL
            AND completed_at_ms IS NULL
            AND detection_level IS NULL
            AND score IS NULL
            AND inference_ms IS NULL
            AND defect_area IS NULL
            AND heatmap_rel_path IS NULL
            AND thumbnail_rel_path IS NULL
            AND error_message IS NULL
        )
    ),

    CHECK (
        status <> 'SUCCESS'
        OR (
            attempt_count >= 1
            AND processing_started_at_ms IS NOT NULL
            AND completed_at_ms IS NOT NULL
            AND detection_level IS NOT NULL
            AND score IS NOT NULL
            AND inference_ms IS NOT NULL
            AND defect_area IS NOT NULL
            AND heatmap_rel_path IS NOT NULL
            AND length(trim(heatmap_rel_path)) > 0
            AND error_message IS NULL
        )
    ),

    CHECK (
        status <> 'FAILED'
        OR (
            attempt_count >= 1
            AND processing_started_at_ms IS NOT NULL
            AND completed_at_ms IS NOT NULL
            AND detection_level IS NULL
            AND score IS NULL
            AND inference_ms IS NULL
            AND defect_area IS NULL
            AND heatmap_rel_path IS NULL
            AND thumbnail_rel_path IS NULL
            AND error_message IS NOT NULL
            AND length(trim(error_message)) > 0
        )
    )
) STRICT;

-- gluttony:statement-break

INSERT INTO inspection_run_v2 (
    run_id, name, note, status, source_type, source_path, peer_address,
    storage_path, storage_origin, model_path, model_format, model_hash,
    app_version, review_threshold, ng_threshold, heatmap_alpha,
    started_at_ms, completed_at_ms, saved_at_ms, last_interrupted_at_ms,
    updated_at_ms, total_count, completed_count, failed_count, ok_count,
    review_count, ng_count, score_sum, received_count, dropped_count,
    manifest_version, manifest_rel_path, manifest_sha256,
    items_rel_path, items_sha256, revision
)
SELECT
    run_id, name, note, status, source_type, source_path, NULL,
    storage_path, storage_origin, model_path, model_format, model_hash,
    app_version, review_threshold, ng_threshold, heatmap_alpha,
    started_at_ms, completed_at_ms, saved_at_ms, last_interrupted_at_ms,
    updated_at_ms, total_count, completed_count, failed_count, ok_count,
    review_count, ng_count, score_sum, total_count, 0,
    manifest_version, manifest_rel_path, manifest_sha256,
    items_rel_path, items_sha256, revision
FROM inspection_run;

-- gluttony:statement-break

INSERT INTO inspection_item_v2 (
    result_id, run_id, sequence_no, source_file_name, source_file_path,
    source_file_size, source_modified_at_ms, source_file_hash,
    transport_sequence, captured_at_ms, pixel_format, image_width,
    image_height, status, detection_level, score, inference_ms,
    defect_area, heatmap_rel_path, thumbnail_rel_path, error_message,
    attempt_count, processing_started_at_ms, completed_at_ms,
    created_at_ms, updated_at_ms
)
SELECT
    result_id, run_id, sequence_no, source_file_name, source_file_path,
    source_file_size, source_modified_at_ms, source_file_hash,
    NULL, NULL, NULL, NULL, NULL, status, detection_level, score,
    inference_ms, defect_area, heatmap_rel_path, thumbnail_rel_path,
    error_message, attempt_count, processing_started_at_ms,
    completed_at_ms, created_at_ms, updated_at_ms
FROM inspection_item;

-- gluttony:statement-break

DROP TABLE inspection_item;

-- gluttony:statement-break

DROP TABLE inspection_run;

-- gluttony:statement-break

ALTER TABLE inspection_run_v2 RENAME TO inspection_run;

-- gluttony:statement-break

ALTER TABLE inspection_item_v2 RENAME TO inspection_item;

-- gluttony:statement-break

CREATE TRIGGER trg_inspection_run_status_transition
BEFORE UPDATE OF status ON inspection_run
WHEN OLD.status <> NEW.status
AND NOT (
       (OLD.status = 'RUNNING'     AND NEW.status = 'DRAFT')
    OR (OLD.status = 'RUNNING'     AND NEW.status = 'INTERRUPTED')
    OR (OLD.status = 'INTERRUPTED' AND NEW.status = 'RUNNING')
    OR (OLD.status = 'DRAFT'       AND NEW.status = 'SAVED')
)
BEGIN
    SELECT RAISE(ABORT, 'invalid inspection_run status transition');
END;

-- gluttony:statement-break

CREATE TRIGGER trg_inspection_run_terminal_check
BEFORE UPDATE OF status ON inspection_run
WHEN NEW.status IN ('DRAFT', 'SAVED')
AND (
    NEW.total_count <= 0
    OR EXISTS (
        SELECT 1
        FROM inspection_item
        WHERE run_id = NEW.run_id
          AND status IN ('PENDING', 'PROCESSING')
    )
    OR (
        SELECT COUNT(*)
        FROM inspection_item
        WHERE run_id = NEW.run_id
    ) <> NEW.total_count
    OR (
        SELECT COUNT(*)
        FROM inspection_item
        WHERE run_id = NEW.run_id
          AND status IN ('SUCCESS', 'FAILED')
    ) <> NEW.completed_count
)
BEGIN
    SELECT RAISE(ABORT, 'inspection_run contains unfinished or inconsistent items');
END;

-- gluttony:statement-break

CREATE TRIGGER trg_inspection_item_status_transition
BEFORE UPDATE OF status ON inspection_item
WHEN OLD.status <> NEW.status
AND NOT (
       (OLD.status = 'PENDING'    AND NEW.status = 'PROCESSING')
    OR (OLD.status = 'PROCESSING' AND NEW.status = 'SUCCESS')
    OR (OLD.status = 'PROCESSING' AND NEW.status = 'FAILED')
    OR (OLD.status = 'PROCESSING' AND NEW.status = 'PENDING')
)
BEGIN
    SELECT RAISE(ABORT, 'invalid inspection_item status transition');
END;

-- gluttony:statement-break

CREATE TRIGGER trg_inspection_item_identity_immutable
BEFORE UPDATE OF
    result_id,
    run_id,
    sequence_no,
    source_file_name,
    source_file_path,
    source_file_size,
    source_modified_at_ms
ON inspection_item
BEGIN
    SELECT RAISE(ABORT, 'inspection_item identity is immutable');
END;

-- gluttony:statement-break

CREATE TRIGGER trg_inspection_item_terminal_immutable
BEFORE UPDATE ON inspection_item
WHEN OLD.status IN ('SUCCESS', 'FAILED')
BEGIN
    SELECT RAISE(ABORT, 'terminal inspection_item is immutable');
END;

-- gluttony:statement-break

CREATE TRIGGER trg_inspection_run_id_immutable
BEFORE UPDATE OF run_id ON inspection_run
BEGIN
    SELECT RAISE(ABORT, 'inspection_run id is immutable');
END;

-- gluttony:statement-break

CREATE TRIGGER trg_inspection_run_saved_immutable
BEFORE UPDATE ON inspection_run
WHEN OLD.status = 'SAVED'
BEGIN
    SELECT RAISE(ABORT, 'saved inspection_run is immutable');
END;

-- gluttony:statement-break

CREATE INDEX idx_inspection_run_saved_history
ON inspection_run(saved_at_ms DESC, run_id)
WHERE status = 'SAVED';

-- gluttony:statement-break

CREATE INDEX idx_inspection_run_recovery
ON inspection_run(status, updated_at_ms DESC);

-- gluttony:statement-break

CREATE INDEX idx_inspection_item_resume
ON inspection_item(run_id, status, sequence_no);

-- gluttony:statement-break

CREATE INDEX idx_inspection_item_level
ON inspection_item(run_id, detection_level, sequence_no)
WHERE status = 'SUCCESS';

-- gluttony:statement-break

CREATE VIEW v_inspection_run_history AS
SELECT
    run_id,
    name,
    note,
    status,
    source_type,
    source_path,
    model_format,
    model_path,
    started_at_ms,
    completed_at_ms,
    saved_at_ms,
    total_count,
    completed_count,
    failed_count,
    ok_count,
    review_count,
    ng_count,
    CASE
        WHEN (ok_count + review_count + ng_count) = 0 THEN NULL
        ELSE score_sum / (ok_count + review_count + ng_count)
    END AS average_score
FROM inspection_run
WHERE status = 'SAVED';

-- gluttony:statement-break

CREATE VIEW v_inspection_run_reconciliation AS
SELECT
    r.run_id,
    r.total_count AS stored_total,
    COUNT(i.result_id) AS actual_total,
    r.completed_count AS stored_completed,
    SUM(CASE
        WHEN i.status IN ('SUCCESS', 'FAILED') THEN 1
        ELSE 0
    END) AS actual_completed,
    r.failed_count AS stored_failed,
    SUM(CASE WHEN i.status = 'FAILED' THEN 1 ELSE 0 END)
        AS actual_failed,
    r.ok_count AS stored_ok,
    SUM(CASE WHEN i.detection_level = 'OK' THEN 1 ELSE 0 END)
        AS actual_ok,
    r.review_count AS stored_review,
    SUM(CASE WHEN i.detection_level = 'REVIEW' THEN 1 ELSE 0 END)
        AS actual_review,
    r.ng_count AS stored_ng,
    SUM(CASE WHEN i.detection_level = 'NG' THEN 1 ELSE 0 END)
        AS actual_ng
FROM inspection_run AS r
LEFT JOIN inspection_item AS i
    ON i.run_id = r.run_id
GROUP BY r.run_id;

-- gluttony:statement-break
