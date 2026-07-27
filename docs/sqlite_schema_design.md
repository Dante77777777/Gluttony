# Gluttony SQLite 数据库结构设计

## 文档信息

| 项目 | 内容 |
|---|---|
| 文档状态 | 设计初稿 |
| 前置文档 | `docs/persistence_requirements.md` |
| 数据库 | SQLite |
| 客户端 | C++ / Qt |
| 初始数据库版本 | 1 |
| 更新时间 | 2026-07-23 |

## 当前落地文件

- 版本 1 迁移：`database/migrations/001_initial_schema.sql`
- 数据库约束测试：`tests/test_sqlite_schema.py`
- 运行说明：`database/README.md`

迁移文件是可执行结构的唯一来源。本文档用于解释设计意图；如果后续通过新迁移修改结构，应同步更新本文档。

## 1. 设计目标

本设计用于实现以下持久化能力：

- 检测开始前保存完整输入清单
- 检测结果逐张落盘
- RUNNING、DRAFT、SAVED、INTERRUPTED 批次状态管理
- PENDING、PROCESSING、SUCCESS、FAILED 条目状态管理
- 中断后的断点续检
- 整个批次的幂等保存
- 从磁盘清单幂等载入批次
- 已保存批次删除
- 数据库记录与热力图文件的一致性恢复
- 大批量数据下的分页查询和固定内存占用

## 2. 设计原则

### 2.1 SQLite 只保存结构化数据

SQLite 保存批次、输入清单、检测结果、状态和文件路径，不保存完整原图或热力图二进制。

热力图保存为 PNG 文件：

```text
data/persistence/runs/{run_id}/heatmaps/{result_id}.png
```

### 2.2 检测条目在推理前创建

为了支持断点续检，`inspection_item` 不是在推理完成后才插入，而是在检测开始前根据完整输入清单预先创建。

```text
PENDING
   ↓ 取得任务
PROCESSING
   ├── SUCCESS
   ├── FAILED
   └── PENDING    程序中断后的恢复
```

### 2.3 SQLite 事务不包含耗时推理和图片编码

模型推理、PNG 编码和普通文件 I/O 不能放在数据库事务内部，否则会长时间占用 SQLite 写锁。

数据库事务只覆盖：

- 状态领取
- 结果提交
- 汇总计数更新
- 批次状态切换
- 导入和删除的原子元数据操作

### 2.4 一个写连接，多个只读连接

SQLite 即使启用 WAL 也只有一个写入者。第一阶段建议：

- 一个持久化工作线程独占写连接
- UI 或历史查询线程各自持有只读连接
- 不在线程之间传递 `QSqlDatabase` 或 `QSqlQuery`
- 所有连接都在创建它们的线程中使用和销毁

## 3. 数据库文件和连接配置

数据库位置：

```text
data/persistence/gluttony.db
```

每个 SQLite 连接创建后必须执行：

```sql
PRAGMA foreign_keys = ON;
PRAGMA busy_timeout = 5000;
```

数据库初始化时执行：

```sql
PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
```

说明：

- `foreign_keys=ON` 是连接级设置，每个连接都必须执行。
- WAL 允许 UI 读取历史数据时，后台继续写入检测结果。
- SQLite 仍然只有一个写入者，因此写操作统一交给持久化线程。
- `busy_timeout` 用于处理短暂写锁竞争，不能代替合理的事务边界。
- 如果未来需要更强的断电持久性，可以将 `synchronous` 调整为 `FULL`，代价是逐条提交速度下降。

建议额外设置固定的 `application_id`，防止用户误选其他 SQLite 文件：

```sql
PRAGMA application_id = 1196183374;
```

`application_id` 的具体整数一旦发布就不能随意修改。

## 4. 通用字段规则

### 4.1 ID

`run_id` 和 `result_id` 使用应用层生成的 UUID，保存为小写标准字符串：

```text
xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
```

数据库只检查非空和长度，完整 UUID 格式由应用层校验。

### 4.2 时间

所有时间统一使用 UTC Unix 毫秒，SQLite 类型为 `INTEGER`。

字段命名使用：

```text
started_at_ms
completed_at_ms
updated_at_ms
```

UI 展示时再根据用户时区转换，数据库中不保存本地时区字符串。

### 4.3 文件路径

- `source_file_path` 保存用户原图的绝对路径。
- `storage_path` 保存批次根目录。
- `heatmap_rel_path`、`thumbnail_rel_path` 保存相对于批次根目录的路径。
- 相对资源路径不得是绝对路径。
- 相对资源路径解析后必须仍位于 `storage_path` 内。
- 数据库 CHECK 不能可靠完成 Windows 路径规范化，必须由存储服务进行规范化和边界校验。

### 4.4 哈希

- ONNX 模型：计算 `.onnx` 文件 SHA-256。
- OpenVINO IR：对 `.xml` 和对应 `.bin` 计算组合 SHA-256。
- `items.jsonl` 和 `batch.json` 分别保存 SHA-256。
- SHA-256 使用 64 位小写十六进制字符串。

## 5. 实体关系

```text
inspection_run
    1
    │ ON DELETE CASCADE
    N
inspection_item

storage_cleanup_job
    独立保存待清理文件或目录
    不使用外键连接已经删除的批次
```

核心表：

1. `schema_migration`：数据库版本历史。
2. `inspection_run`：一次检测批次。
3. `inspection_item`：批次中的输入图片和检测结果。
4. `storage_cleanup_job`：数据库提交后仍需执行的文件删除任务。

## 6. 完整 DDL

以下 DDL 使用 SQLite `STRICT` 表，需要 SQLite 3.37 或更高版本。程序初始化时必须先查询：

```sql
SELECT sqlite_version();
```

如果目标运行环境不支持 `STRICT`，应在迁移脚本中移除 `STRICT`，但保留所有 `CHECK` 约束。

### 6.1 数据库迁移表

```sql
CREATE TABLE schema_migration (
    version         INTEGER PRIMARY KEY,
    name            TEXT NOT NULL UNIQUE,
    checksum        TEXT NOT NULL,
    applied_at_ms   INTEGER NOT NULL,

    CHECK (version > 0),
    CHECK (length(trim(name)) > 0),
    CHECK (length(checksum) = 64),
    CHECK (applied_at_ms > 0)
) STRICT;
```

`checksum` 是迁移 SQL 文件的 SHA-256。已经发布并执行的迁移文件不得修改。

### 6.2 检测批次表

```sql
CREATE TABLE inspection_run (
    run_id                  TEXT PRIMARY KEY,
    name                    TEXT,
    note                    TEXT,
    status                  TEXT NOT NULL,

    source_type             TEXT NOT NULL,
    source_path             TEXT NOT NULL,

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
    CHECK (source_type IN ('SINGLE_IMAGE', 'FOLDER')),
    CHECK (length(trim(source_path)) > 0),
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
```

设计说明：

- `average_score` 不直接存储，使用 `score_sum / (ok_count + review_count + ng_count)` 计算。
- 统计字段属于有意的反规范化，用于快速显示批次列表。
- 检测条目提交结果时，必须在同一个事务中更新统计字段。
- `revision` 用于检测并发修改，更新名称、备注或状态时递增。
- `storage_origin=MANAGED` 表示文件由当前应用管理。
- `storage_origin=EXTERNAL` 表示从外部 `batch.json` 载入，默认删除时只删除本地数据库索引。

### 6.3 检测条目表

```sql
CREATE TABLE inspection_item (
    result_id              TEXT PRIMARY KEY,
    run_id                 TEXT NOT NULL,
    sequence_no            INTEGER NOT NULL,

    source_file_name       TEXT NOT NULL,
    source_file_path       TEXT NOT NULL,
    source_file_size       INTEGER NOT NULL,
    source_modified_at_ms  INTEGER NOT NULL,
    source_file_hash       TEXT,

    status                 TEXT NOT NULL,
    detection_level        TEXT,
    score                  REAL,
    inference_ms           INTEGER,
    defect_area            INTEGER,

    heatmap_rel_path       TEXT,
    thumbnail_rel_path     TEXT,
    error_message          TEXT,

    attempt_count          INTEGER NOT NULL DEFAULT 0,
    processing_started_at_ms INTEGER,
    completed_at_ms        INTEGER,
    created_at_ms          INTEGER NOT NULL,
    updated_at_ms          INTEGER NOT NULL,

    FOREIGN KEY (run_id)
        REFERENCES inspection_run(run_id)
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
```

设计说明：

- `sequence_no` 从 1 开始，表示用户本次输入清单中的固定顺序。
- 同一路径可以在同一批次出现多次，因此不能对 `(run_id, source_file_path)` 建立唯一约束。
- `source_file_size` 和 `source_modified_at_ms` 用于续检前判断原图是否发生变化。
- `source_file_hash` 第一阶段可以为空，后续可升级为完整内容校验。
- `PENDING` 条目的 `attempt_count` 可以大于 0，因为崩溃恢复会把 `PROCESSING` 重置为 `PENDING`，但保留历史尝试次数。
- 失败条目不保存部分推理结果，避免出现“状态失败但仍有可用分数”的含糊语义。

### 6.4 文件清理任务表

```sql
CREATE TABLE storage_cleanup_job (
    job_id                  INTEGER PRIMARY KEY,
    target_path             TEXT NOT NULL UNIQUE,
    target_kind             TEXT NOT NULL,
    reason                  TEXT NOT NULL,
    origin_run_id           TEXT,

    attempt_count           INTEGER NOT NULL DEFAULT 0,
    last_attempt_at_ms      INTEGER,
    last_error              TEXT,
    created_at_ms           INTEGER NOT NULL,

    CHECK (length(trim(target_path)) > 0),
    CHECK (target_kind IN ('FILE', 'DIRECTORY')),
    CHECK (reason IN (
        'DISCARD_DRAFT',
        'DELETE_SAVED',
        'ROLLBACK_FILE',
        'ORPHAN_FILE'
    )),
    CHECK (
        origin_run_id IS NULL
        OR length(origin_run_id) = 36
    ),
    CHECK (attempt_count >= 0),
    CHECK (created_at_ms > 0),
    CHECK (
        last_attempt_at_ms IS NULL
        OR last_attempt_at_ms >= created_at_ms
    )
) STRICT;
```

这个表故意不对 `inspection_run` 建立外键。删除批次时，需要先记录待清理目录，再删除批次行；事务提交后批次已经不存在，但清理任务必须继续存在。

文件清理服务执行前必须再次验证：

1. 目标经过规范化后的绝对路径位于应用管理根目录内。
2. 目标不是应用管理根目录本身。
3. 目标不是磁盘根目录、用户目录或工作区根目录。
4. `storage_origin=EXTERNAL` 的目录不能自动加入删除任务。

## 7. 状态转换触发器

CHECK 约束只能验证一行最终是否合法，不能限制状态从哪里转换而来。因此建议增加以下触发器。

### 7.1 批次状态转换

允许的状态转换：

```text
RUNNING     → DRAFT
RUNNING     → INTERRUPTED
INTERRUPTED → RUNNING
DRAFT       → SAVED
```

```sql
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
```

### 7.2 批次完成检查

```sql
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
```

### 7.3 条目状态转换

允许的状态转换：

```text
PENDING    → PROCESSING
PROCESSING → SUCCESS
PROCESSING → FAILED
PROCESSING → PENDING
```

最后一种只用于崩溃恢复。

```sql
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
```

### 7.4 条目身份不可修改

```sql
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
```

如果用户重新选择或移动了原图，应创建新批次，不能修改旧批次输入清单。

### 7.5 终态结果不可修改

```sql
CREATE TRIGGER trg_inspection_item_terminal_immutable
BEFORE UPDATE ON inspection_item
WHEN OLD.status IN ('SUCCESS', 'FAILED')
BEGIN
    SELECT RAISE(ABORT, 'terminal inspection_item is immutable');
END;
```

第一阶段不支持对失败条目单独重试。如果未来增加单条重试，需要通过新的迁移修改状态机，不能直接删除这个保护。

### 7.6 批次 ID 不可修改

```sql
CREATE TRIGGER trg_inspection_run_id_immutable
BEFORE UPDATE OF run_id ON inspection_run
BEGIN
    SELECT RAISE(ABORT, 'inspection_run id is immutable');
END;
```

### 7.7 已保存批次不可修改

```sql
CREATE TRIGGER trg_inspection_run_saved_immutable
BEFORE UPDATE ON inspection_run
WHEN OLD.status = 'SAVED'
BEGIN
    SELECT RAISE(ABORT, 'saved inspection_run is immutable');
END;
```

第一阶段的名称和备注在保存时一次性确定。保存后批次内容不可修改，只能查看或删除。这样可以保证 SQLite、`batch.json` 和 `items.jsonl` 始终保持一致。

## 8. 索引设计

### 8.1 已保存批次列表

```sql
CREATE INDEX idx_inspection_run_saved_history
ON inspection_run(saved_at_ms DESC, run_id)
WHERE status = 'SAVED';
```

支持：

```text
按保存时间倒序分页显示历史批次
```

### 8.2 恢复批次查询

```sql
CREATE INDEX idx_inspection_run_recovery
ON inspection_run(status, updated_at_ms DESC);
```

支持启动时查询 `RUNNING` 和 `INTERRUPTED` 批次。

### 8.3 断点续检

```sql
CREATE INDEX idx_inspection_item_resume
ON inspection_item(run_id, status, sequence_no);
```

支持：

```text
查询某批次所有 PENDING 条目并按原始顺序继续
```

### 8.4 按检测等级筛选

```sql
CREATE INDEX idx_inspection_item_level
ON inspection_item(run_id, detection_level, sequence_no)
WHERE status = 'SUCCESS';
```

支持在批次详情中筛选 OK、REVIEW 或 NG。

### 8.5 清理任务

```sql
CREATE INDEX idx_storage_cleanup_job_created
ON storage_cleanup_job(created_at_ms, job_id);
```

用于按创建顺序执行待清理任务。

### 8.6 已有唯一索引

下面的约束会自动建立索引，不需要重复创建：

```text
inspection_run(run_id)
inspection_item(result_id)
inspection_item(run_id, sequence_no)
storage_cleanup_job(target_path)
```

`UNIQUE(run_id, sequence_no)` 已经支持按批次顺序分页，因此不再额外建立 `(run_id, sequence_no)` 普通索引。

第一阶段不建议立即为文件名、原图路径、分数和缺陷面积全部建索引。索引会增加逐条写入成本，应在 UI 确认存在对应筛选需求后再增加。

## 9. 查询视图

### 9.1 批次历史视图

```sql
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
```

### 9.2 统计一致性检查视图

```sql
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
    SUM(CASE WHEN i.status = 'FAILED' THEN 1 ELSE 0 END) AS actual_failed,
    r.ok_count AS stored_ok,
    SUM(CASE WHEN i.detection_level = 'OK' THEN 1 ELSE 0 END) AS actual_ok,
    r.review_count AS stored_review,
    SUM(CASE WHEN i.detection_level = 'REVIEW' THEN 1 ELSE 0 END)
        AS actual_review,
    r.ng_count AS stored_ng,
    SUM(CASE WHEN i.detection_level = 'NG' THEN 1 ELSE 0 END) AS actual_ng
FROM inspection_run AS r
LEFT JOIN inspection_item AS i
    ON i.run_id = r.run_id
GROUP BY r.run_id;
```

这个视图用于测试、维护和故障诊断，不应在每次 UI 刷新时全表执行。

## 10. 事务边界

### 10.1 数据库初始化和迁移

连接级 PRAGMA 设置完成后：

1. 检查 `application_id`。
2. 读取 `PRAGMA user_version`。
3. 验证 `schema_migration` 中已执行迁移的 checksum。
4. 对每个待执行迁移开启独立事务。

```text
BEGIN IMMEDIATE
    执行一个版本的 DDL
    插入 schema_migration
    设置 PRAGMA user_version
COMMIT
```

迁移失败必须整体回滚。不能在同一个事务中修改 `journal_mode`。

### 10.2 创建批次和输入清单

目标：数据库一旦出现 `RUNNING` 批次，就必须已经具有完整输入清单。

推荐流程：

1. 创建受管理批次目录。
2. 开启 `BEGIN IMMEDIATE`。
3. 插入 `inspection_run`，初始 `total_count=0`。
4. 枚举输入文件，发现一个有效文件就插入一个 `PENDING` 条目。
5. 不把完整输入路径列表保存在内存中。
6. 枚举结束后，如果有效图片数量为 0，则回滚事务。
7. 更新批次 `total_count` 为实际输入数量。
8. 提交事务。
9. 提交成功后才启动推理工作线程。

第一阶段是单机单写者，允许在创建批次时使用一个相对较长的写事务。目录枚举和条目插入应在持久化工作线程中执行，不能阻塞 UI 线程。WAL 模式下已有历史记录仍可被只读连接查询。

如果事务失败：

- 数据库中不留下批次或部分输入清单。
- 已创建的空批次目录加入清理任务或立即安全删除。

### 10.3 领取下一条任务

推理线程不能直接根据内存下标假设任务状态。领取任务时使用短事务：

```text
BEGIN IMMEDIATE
    查询 run_id 下 sequence_no 最小的 PENDING 条目
    将该条目更新为 PROCESSING
    attempt_count += 1
    写入 processing_started_at_ms 和 updated_at_ms
COMMIT
```

事务提交后再读取原图并执行模型推理。

即使第一阶段只有一个推理线程，也保留“领取”事务，方便未来扩展多个工作线程。

### 10.4 成功结果提交

事务外执行：

1. 模型推理。
2. 将热力图编码到 `{result_id}.png.tmp`。
3. 确认写入完成。
4. 原子重命名为 `{result_id}.png`。

然后开启短事务：

```text
BEGIN IMMEDIATE
    确认批次仍为 RUNNING
    确认条目仍为 PROCESSING
    将条目更新为 SUCCESS
    写入分数、等级、耗时、缺陷面积和热力图相对路径
    根据等级更新 inspection_run 计数
    completed_count += 1
    score_sum += score
    updated_at_ms = 当前时间
    revision += 1
COMMIT
```

更新条目时必须带条件：

```text
WHERE result_id = ? AND status = 'PROCESSING'
```

受影响行数必须等于 1。否则回滚，并把新生成的 PNG 加入清理任务。

数据库事务失败时：

1. 不得把该条目视为完成。
2. 尝试删除刚生成的 PNG。
3. 删除失败时写入 `storage_cleanup_job`。
4. 根据错误类型重试数据库提交或中断批次。

### 10.5 失败结果提交

图片不存在、图片损坏、推理失败或热力图保存失败时：

```text
BEGIN IMMEDIATE
    确认条目仍为 PROCESSING
    将条目更新为 FAILED
    写入 error_message 和 completed_at_ms
    failed_count += 1
    completed_count += 1
    更新批次 updated_at_ms 和 revision
COMMIT
```

失败提交不写入部分分数和检测等级。

### 10.6 暂停和正常中断

暂停只是不再领取新的 `PENDING` 条目。当前 `PROCESSING` 条目应先完成成功或失败提交。

用户关闭正在检测的任务时：

```text
停止领取新任务
等待当前 PROCESSING 条目完成
BEGIN IMMEDIATE
    RUNNING → INTERRUPTED
    写入 last_interrupted_at_ms
    更新 revision
COMMIT
```

如果不能等待而进程直接结束，启动恢复流程会处理遗留状态。

### 10.7 启动时恢复中断批次

程序启动后使用一个事务：

```text
BEGIN IMMEDIATE
    将所有 RUNNING 批次更新为 INTERRUPTED
    将这些批次下的 PROCESSING 条目重置为 PENDING
    processing_started_at_ms = NULL
    保留 attempt_count
    更新 last_interrupted_at_ms 和 updated_at_ms
COMMIT
```

恢复后由用户决定“继续检测”或“放弃”。

继续检测前，在事务外校验：

- 模型文件存在
- 模型哈希一致
- PENDING 原图存在
- 原图大小和修改时间与输入清单一致

校验完成后用短事务执行：

```text
INTERRUPTED → RUNNING
```

然后重新开始领取 `PENDING` 条目。已经处于 `SUCCESS` 或 `FAILED` 的条目不得再次领取。

### 10.8 批次完成

当没有 `PENDING` 或 `PROCESSING` 条目后：

```text
BEGIN IMMEDIATE
    从 inspection_item 重新聚合实际统计
    用实际统计覆盖 inspection_run 中的缓存统计
    RUNNING → DRAFT
    写入 completed_at_ms
    更新 revision
COMMIT
```

结束时重新聚合一次，可以修正增量计数中可能出现的错误。完成检查触发器会阻止存在未完成条目的批次进入 `DRAFT`。

### 10.9 保存整个批次

保存操作分成文件阶段和数据库阶段。

事务外：

1. 校验批次为 `DRAFT`。
2. 校验批次名称非空。
3. 分页查询检测条目并流式写入 `items.jsonl.tmp`。
4. 计算 `items.jsonl.tmp` 的文件大小和 SHA-256。
5. 写入 `batch.json.tmp`。
6. 计算 `batch.json.tmp` 的 SHA-256。
7. 原子重命名两个临时清单文件。

随后开启短事务：

```text
BEGIN IMMEDIATE
    再次确认批次为 DRAFT
    DRAFT → SAVED
    写入名称、备注、saved_at_ms
    写入 manifest 和 items 文件路径及 SHA-256
    更新 revision
COMMIT
```

如果数据库事务失败：

- 批次保持 `DRAFT`。
- 删除本次生成的 `batch.json` 和 `items.jsonl`。
- 删除失败时加入 `storage_cleanup_job`。

重复保存时：

- 如果批次已经是 `SAVED` 且清单哈希一致，直接返回成功。
- 不重新插入批次或检测条目。
- 如果清单与数据库哈希冲突，报告一致性错误，不静默覆盖。

### 10.10 不保存草稿

对于 `MANAGED` 的 `DRAFT`：

```text
BEGIN IMMEDIATE
    插入批次目录的 storage_cleanup_job
    删除 inspection_run
    inspection_item 通过 ON DELETE CASCADE 删除
COMMIT
```

事务提交后执行目录删除。删除成功后删除清理任务；删除失败则更新清理任务的尝试次数和错误信息，下次启动继续处理。

先提交数据库删除任务、再删除文件，是为了避免出现文件已经删除但数据库事务回滚的情况。

### 10.11 删除已保存批次

`storage_origin=MANAGED`：

```text
BEGIN IMMEDIATE
    插入目录清理任务，reason=DELETE_SAVED
    删除 inspection_run
COMMIT
事务外删除目录
```

`storage_origin=EXTERNAL`：

```text
BEGIN IMMEDIATE
    只删除 inspection_run
COMMIT
```

外部批次默认只从当前历史索引中移除，不能自动删除用户选择的外部目录。

### 10.12 从磁盘载入批次

载入分成验证和导入两个阶段。

事务外第一次流式读取：

1. 校验 `batch.json` 格式版本。
2. 校验 `run_id` 和必需字段。
3. 校验 `items.jsonl` 文件大小和 SHA-256。
4. 逐行校验 JSON、条目数量、UUID、状态和相对路径。
5. 校验资源相对路径解析后仍位于外部批次目录。

验证通过后，第二次流式读取并开启单一导入事务：

```text
BEGIN IMMEDIATE
    如果 run_id 已存在：
        比较 manifest_sha256 和 items_sha256
        相同则回滚本次导入并直接打开已有批次
        不同则报告冲突并回滚

    临时插入 DRAFT 的 inspection_run
    storage_origin = EXTERNAL
    storage_path = 用户选择的批次目录

    逐行插入 inspection_item
    校验最终条目数量和统计

    DRAFT → SAVED
    写入清单路径、哈希和 saved_at_ms
COMMIT
```

虽然逐行读取，但整个导入只使用一个数据库事务，从而保证“全部导入或全部回滚”。不要为了降低内存使用而拆成多个可见事务；流式读取本身已经可以控制内存。

### 10.13 名称和备注写入边界

名称和备注在 `DRAFT → SAVED` 的保存事务中写入。第一阶段不提供保存后的编辑操作。

如果未来需要编辑已保存批次，应通过新数据库版本增加清单同步状态和失败恢复流程，不能只更新数据库字段而忽略磁盘清单。

## 11. 读取和分页边界

### 11.1 历史批次分页

第一阶段可以使用基于游标的分页：

```sql
SELECT *
FROM v_inspection_run_history
WHERE
    saved_at_ms < :last_saved_at_ms
    OR (
        saved_at_ms = :last_saved_at_ms
        AND run_id < :last_run_id
    )
ORDER BY saved_at_ms DESC, run_id DESC
LIMIT :page_size;
```

第一页不传游标条件。推荐 `page_size=50`。

### 11.2 批次条目分页

```sql
SELECT
    result_id,
    sequence_no,
    source_file_name,
    source_file_path,
    status,
    detection_level,
    score,
    inference_ms,
    defect_area,
    heatmap_rel_path,
    thumbnail_rel_path,
    error_message
FROM inspection_item
WHERE run_id = :run_id
  AND sequence_no > :last_sequence_no
ORDER BY sequence_no
LIMIT :page_size;
```

推荐 `page_size=100`。UI 表格只保存当前需要的轻量行数据，不预加载完整图片。

### 11.3 图片按需加载

数据库读取事务在得到路径后立即结束。PNG 文件读取、解码和显示在事务外进行。

```text
数据库查询路径
COMMIT / 结束只读语句
读取 PNG
解码 QImage
显示
```

不能在持有数据库写事务时等待磁盘图片解码。

## 12. 并发和线程规则

### 12.1 写入线程

持久化工作线程负责：

- 创建批次
- 领取任务状态
- 提交成功或失败结果
- 更新批次状态
- 保存和导入批次
- 创建文件清理任务

所有写命令串行执行。

### 12.2 UI 线程

UI 线程只负责：

- 提交持久化命令
- 查询轻量历史数据
- 展示执行结果
- 按需触发图片读取

UI 线程不得执行大型导入、全表聚合或递归文件删除。

### 12.3 推理线程

推理线程负责模型推理，不直接持有数据库写连接。它通过明确的数据传输对象把结果交给持久化线程。

结果只有在持久化事务成功后，才算正式完成。UI 中的完成数量应以持久化确认信号为准，而不是以模型推理结束信号为准。

## 13. 错误处理规则

### 13.1 SQLite busy

- 每个连接设置 `busy_timeout=5000`。
- 超时后将错误返回调用方。
- 只有幂等操作才能自动重试。
- 自动重试必须设置次数上限，不能无限循环。

### 13.2 磁盘空间不足

热力图临时文件写入失败时：

- 当前条目更新为 `FAILED`。
- 错误信息明确记录“热力图写入失败”。
- 后续条目是否继续由产品策略决定；建议磁盘空间不足时中断整个批次，避免产生大量失败结果。

### 13.3 数据库损坏

程序启动时可以执行：

```sql
PRAGMA quick_check;
```

如果结果不是 `ok`：

- 禁止继续写入。
- 提示用户备份当前数据库和 WAL 文件。
- 不自动删除或重建数据库。

### 13.4 清理失败

文件删除失败不会回滚已经提交的数据库删除。清理任务保留在 `storage_cleanup_job`，下次启动继续执行。

### 13.5 外部批次冲突

相同 `run_id` 但清单哈希不同，必须视为冲突：

- 不覆盖现有批次。
- 不生成新的 `run_id` 冒充不同批次。
- 显示现有路径、外部路径和哈希差异。

## 14. 数据库迁移策略

### 14.1 版本来源

同时使用：

- `PRAGMA user_version`：快速读取当前版本。
- `schema_migration`：保存版本名称、checksum 和应用时间。

两个版本来源不一致时停止启动并报告迁移状态异常。

### 14.2 迁移文件

建议命名：

```text
001_initial_schema.sql
002_add_example_field.sql
```

迁移文件一旦发布，不得修改。新结构必须通过新迁移实现。

### 14.3 SQLite 表重建

遇到 SQLite 不能直接完成的字段修改时：

1. 开启事务。
2. 创建新表。
3. 验证并复制旧数据。
4. 删除旧表。
5. 重命名新表。
6. 重建索引、视图和触发器。
7. 执行外键检查。
8. 更新版本。
9. 提交事务。

迁移结束前执行：

```sql
PRAGMA foreign_key_check;
```

## 15. 备份和维护

### 15.1 数据库备份

WAL 模式下不能简单复制正在使用的 `.db` 文件作为可靠备份。应使用：

- SQLite Backup API
- `VACUUM INTO`
- 程序完全关闭并确认 WAL 已合并后复制

### 15.2 批次备份

一个可从磁盘载入的完整批次至少包含：

```text
{run_id}/
├── batch.json
├── items.jsonl
└── heatmaps/
```

原图没有复制到批次目录，因此批次备份可能只能显示热力图，这是已经确认的第一阶段行为。

### 15.3 WAL 检查点

程序正常退出时可以执行被动检查点：

```sql
PRAGMA wal_checkpoint(PASSIVE);
```

不要在每张图片完成后执行检查点。

### 15.4 优化

大量删除或导入完成后，可在空闲时执行：

```sql
PRAGMA optimize;
```

不要在检测主流程中频繁执行 `VACUUM`。

## 16. 验收检查

数据库结构完成后必须验证：

1. 非法批次状态转换被触发器拒绝。
2. 非法条目状态转换被触发器拒绝。
3. 已保存批次不能被修改。
4. `DRAFT` 或 `SAVED` 批次不能存在 PENDING 或 PROCESSING 条目。
5. 删除批次会级联删除条目。
6. 重复 `(run_id, sequence_no)` 被拒绝。
7. 重复保存不会新增批次或条目。
8. 崩溃恢复会把 PROCESSING 重置为 PENDING。
9. SUCCESS 条目缺少热力图路径时写入失败。
10. FAILED 条目缺少错误信息时写入失败。
11. 外部批次删除时不会生成外部目录清理任务。
12. 相同清单重复载入不会重复插入。
13. 相同 `run_id`、不同哈希的清单会被拒绝。
14. 10,000 条输入清单可以在固定图片内存下创建和分页查询。
15. `v_inspection_run_reconciliation` 中的缓存统计与实际统计一致。
16. 所有连接都启用了外键约束。

## 17. 推荐实施顺序

1. 确认目标 SQLite 版本是否支持 `STRICT`。
2. 创建版本 1 迁移文件。
3. 在临时数据库执行完整 DDL。
4. 为 CHECK、外键和触发器编写数据库级测试。
5. 实现数据库初始化和连接配置。
6. 实现批次与输入清单事务。
7. 实现任务领取和结果提交事务。
8. 实现启动恢复和断点续检。
9. 实现批次保存与磁盘清单。
10. 实现草稿和历史批次删除。
11. 实现磁盘批次载入。
12. 最后接入 UI 历史列表和图片按需加载。

在数据库事务测试完成以前，不应直接把写入逻辑接入现有推理线程。
