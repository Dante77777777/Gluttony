# Gluttony 数据库目录

## 目录结构

```text
database/
└── migrations/
    └── 001_initial_schema.sql
```

`001_initial_schema.sql` 是数据库版本 1 的完整结构迁移，包含：

- 4 张表
- 5 个业务索引
- 7 个状态和不可变性触发器
- 2 个查询及核对视图

## 运行环境

版本 1 使用 SQLite `STRICT` 表，需要 SQLite 3.37 或更高版本。

可以使用 Python 标准库检查当前测试环境：

```powershell
python -c "import sqlite3; print(sqlite3.sqlite_version)"
```

Qt 客户端接入前，也需要通过 `QSQLITE` 连接执行：

```sql
SELECT sqlite_version();
```

## 迁移文件职责

迁移 SQL 只负责创建数据库对象。未来的 Qt 迁移执行器负责：

1. 打开数据库连接。
2. 对每个连接启用 `PRAGMA foreign_keys = ON`。
3. 设置 `PRAGMA busy_timeout = 5000`。
4. 首次初始化时设置 WAL 和同步模式。
5. 计算迁移文件 SHA-256。
6. 开启迁移事务。
7. 执行迁移 SQL。
8. 写入 `schema_migration`。
9. 更新 `PRAGMA user_version`。
10. 提交或整体回滚。

不要在迁移 SQL 中硬编码自身 checksum，因为修改文件内容会同时改变 checksum。

## 运行数据库测试

测试只依赖 Python 标准库，不需要安装 pytest：

```powershell
python -m unittest discover -s tests -v
```

测试使用内存数据库，不会在工作区创建真实的 `gluttony.db`。

当前覆盖：

- 数据库对象和迁移元数据
- 阈值和保存字段 CHECK 约束
- 外键和级联删除
- 批次输入清单事务回滚
- `(run_id, sequence_no)` 唯一约束
- 条目状态转换
- 批次完成检查
- 保存和历史查询
- 统计核对视图
- 中断恢复
- 已保存批次和终态条目不可修改
- 清理任务路径唯一性

## 相关文档

- [持久化需求与概要设计](../docs/persistence_requirements.md)
- [SQLite 数据库结构设计](../docs/sqlite_schema_design.md)

## Qt/C++ 落地状态

当前已经实现：

1. Qt SQL 组件和 QSQLITE 连接配置。
2. 数据库与受管理批次目录路径管理。
3. SQLite 版本、外键、busy timeout、WAL 和同步级别校验。
4. 内嵌迁移目录、语句分隔、UTF-8/LF 规范化和 SHA-256。
5. 迁移历史规划、checksum 校验和逐版本 `BEGIN IMMEDIATE` 执行。
6. `schema_migration`、`user_version`、`application_id` 和外键后置检查。
7. 启动时将遗留 RUNNING 批次恢复为 INTERRUPTED。
8. 批次与输入清单创建仓储。
9. PENDING 领取、SUCCESS/FAILED 结果提交仓储。
10. 批次完成、保存幂等、分页读取、草稿/历史删除和清理任务。
11. 使用临时数据库运行的 C++ 仓储冒烟测试。
12. 专用 `PersistenceWorker` 线程和线程内 SQLite 连接生命周期。
13. 文件夹流式枚举、数据库任务领取和逐条推理调度。
14. 热力图 PNG 临时写入、原子提交和失败回滚。
15. 只保存 ID、指标和磁盘路径的轻量结果模型。
16. 草稿保存/放弃、JSON manifest、历史加载和已保存批次删除。
17. RUNNING 批次中断和 INTERRUPTED 批次断点续检。
18. 覆盖完整工作器生命周期的磁盘落盘集成测试。

核心实现位于：

```text
clients/Gluttony/DataBase/
```

C++ 测试可以通过 CTest 运行：

```powershell
ctest --test-dir clients/Gluttony/build/<kit-build-directory> `
    -C Release --output-on-failure
```

现有 Python 数据库约束测试仍可通过以下命令运行：

```powershell
python -m unittest discover -s tests -v
```

UI 和推理线程不会直接持有数据库连接。检测任务在 SQLite 中从
`PENDING` 转为 `PROCESSING` 后才进入推理线程；热力图完成 PNG 原子落盘并且
结果事务提交成功后，UI 才显示该条记录。历史记录按路径从磁盘加载热力图，
结果表不长期保存完整 `QImage`。
