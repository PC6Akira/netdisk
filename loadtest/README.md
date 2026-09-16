# 压测脚本（JMeter）

## 预置数据

1. 启动应用 + MySQL（`docker compose up -d` + `mvn spring-boot:run`）。
2. 执行预置脚本：

```bash
bash loadtest/seed.sh
```

生成 `/tmp/loadtest/users.csv`（100 用户：`username,password,fileId`）和 1000 个本地上传文件。

## 跑压测

```bash
jmeter -n -t loadtest/netdisk-loadtest.jmx -l results.jtl -e -o report
```

- 1000 线程、ramp-up 1s、循环 1 次，每线程执行「登录 → 列表 → 下载 → 预览 → 上传 → 配额」。
- HTML 报告在 `report/index.html`，分位数在 `report/statistics.json`。

## 参数

| 项 | 位置 | 说明 |
|---|---|---|
| 线程数 / ramp | JMX 里 `ThreadGroup.num_threads` / `ramp_time` | 默认 1000 / 1s |
| 用户 CSV 路径 | `CSVDataSet.filename` | 默认 `/tmp/loadtest/users.csv`，硬编码，按需改 |
| 上传文件路径 | 上传 sampler 的 `File.path` | 默认 `/tmp/loadtest/upfiles/upload_${__threadNum}.txt` |

> 结论与两轮线程池调优对比见 [`docs/loadtest-report.md`](../docs/loadtest-report.md)。
