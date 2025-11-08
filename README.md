# SysMon — C++17 System Monitor (WSL/Linux)

A lightweight system monitor written in C++17. Shows live CPU/Mem and a per-process table (like `top`), plus a REST API and an embedded web dashboard.

## Features
- TUI with CPU %, Memory, PID/Name/CPU%/Mem MB
- Sort (`s cpu`, `s mem`), filter (`f <text>`), kill (`k <pid>`, `k! <pid>`)
- REST: `GET /api/metrics`, `GET /api/procs`
- Web UI at `http://127.0.0.1:8080/` (served by the app)

## Build & Run
```bash
sudo apt update && sudo apt install -y build-essential
make
./bin/sysmon --api

