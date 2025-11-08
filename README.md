# 🖥️ SysMon — C++17 System Monitor for Linux/WSL

A lightweight **system monitor** written in **C++17**, designed for **Linux/WSL environments**.  
SysMon provides both a **Terminal User Interface (TUI)** and a **REST API + Web Dashboard** for real-time system metrics visualization — CPU, memory, and process data — similar to tools like `top` and `htop`.

---

## 🚀 Features

✅ **Live Metrics Display**
- CPU utilization percentage (per core + total)
- Memory usage summary
- Process count and list (PID, CPU%, Memory%)

✅ **Terminal-based System Monitor**
- Interactive TUI (`sysmon` command)
- Sort/filter processes (`s cpu`, `s mem`, `f <text>`)
- Kill processes (`k <pid>` or `k! <pid>`)

✅ **Built-in REST API (localhost)**
- `GET /api/metrics` → Returns system summary (CPU%, Mem%, etc.)
- `GET /api/procs` → Lists top processes
- Serves embedded HTML dashboard at `http://127.0.0.1:8080/`

✅ **Web Dashboard**
- Clean, responsive UI showing live system stats
- Auto-refresh every 1 second
- Built using minimal HTML, CSS & JS

✅ **Platform Support**
- Runs natively on **Linux** or **Windows Subsystem for Linux (WSL2)**

---

## 🧩 Tech Stack

| Component | Technology Used |
|------------|------------------|
| Core Logic | C++17 |
| Build Tool | Makefile |
| API Server | Lightweight HTTP in C++ |
| UI | HTML5, CSS3, JavaScript |
| Platform | Ubuntu on WSL2 |
| Automation | GitHub Actions (CI/CD ready) |

---

## ⚙️ Installation & Setup

### 1️⃣ Prerequisites
```bash
sudo apt update && sudo apt install -y build-essential git
