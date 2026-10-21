# AegisOS — Linux Process Monitoring and Supervision

## 1. Project Overview

AegisOS is a C-based Linux process monitoring and supervision system developed as an Operating Systems laboratory project.

It monitors running processes, collects CPU and memory usage information, detects sustained resource usage anomalies, and provides a process supervisor that can restart programs when they exit unsuccessfully.

The project includes a command-line interface (CLI), an HTTP API, a browser dashboard, configurable policies, and automated tests.

**Important:** Anomaly detection and process supervision currently operate separately. Detecting high CPU or memory usage does not automatically restart the affected process.

## 2. Features

- Monitor running Linux processes.
- Collect CPU and memory usage information.
- Detect sustained high CPU and memory usage.
- Configure anomaly thresholds and detection settings.
- Launch and supervise child processes.
- Retry failed programs using restart limits and exponential backoff.
- Apply recovery policy decisions.
- Provide an HTTP API that returns JSON data.
- Display process information and anomalies through a web dashboard.
- Generate CSV reports from supplied report data.
- Run automated tests using Make.

## 3. Technologies Used

- **C:** Main implementation language.
- **Linux:** Operating system used for process monitoring and supervision.
- **GCC:** C compiler.
- **GNU Make:** Build automation.
- **HTML, CSS, and JavaScript:** Dashboard interface.
- **HTTP and JSON:** Communication between the backend and dashboard.
- **Bash:** Terminal commands and test scripts.
- **Git and GitHub:** Version control.

## 4. Requirements

Install or ensure you have the following in Ubuntu or Ubuntu on WSL2:

- GCC
- GNU Make
- Python 3
- A web browser

Check whether the tools are installed:

```bash
gcc --version
make --version
python3 --version
```

## 5. Build the Project

Open an Ubuntu terminal and navigate to the project directory:

```bash
cd /mnt/c/Users/idunn/Downloads/OS-Project-member-2
```

Build the project:

```bash
make clean
make
```

The compiled executable is located at:

```text
build/aegisos
```

Check the available commands:

```bash
./build/aegisos --help
```

Check the version:

```bash
./build/aegisos --version
```

## 6. Run the Tests

From the project root, run:

```bash
make test
```

The current test suite has reported 11 passed and 0 failed.

The tests cover selected behaviors, including invalid inputs, process monitoring, and process supervision. Passing tests do not necessarily mean every dashboard feature or recovery workflow is implemented.

## 7. Process Monitoring

AegisOS can monitor an existing Linux process using its PID.

First, start a temporary process:

```bash
sleep 60 &
PID=$!
echo "$PID"
```

Monitor it for three samples:

```bash
./build/aegisos --monitor "$PID" 3
```

The command displays information such as:

- Process ID
- Process name
- Process state
- CPU usage
- Memory usage

The monitor samples approximately once per second. If the sample count is omitted, the default is 10 samples.

Stop the temporary process when finished:

```bash
kill "$PID"
```

## 8. Process Supervision

AegisOS can launch a program and supervise its execution.

General syntax:

```bash
./build/aegisos --supervise PROGRAM [ARGS...]
```

To test a program that exits successfully:

```bash
./build/aegisos --supervise sh -c 'exit 0'
```

To demonstrate restart attempts after an unsuccessful exit:

```bash
./build/aegisos --supervise sh -c 'exit 1'
```

The second command intentionally exits with a failure status. The supervisor attempts to restart it according to its configured limits.

The current CLI supervisor uses:

- Maximum restart attempts: 3
- Initial backoff delay: 1 second
- Maximum backoff delay: 60 seconds

The delay increases between successive attempts. When the restart limit is reached, the supervisor stops retrying.

Use a test program for this demonstration rather than a critical application.

## 9. Start the HTTP API

Open a terminal in the project directory and run:

```bash
./build/aegisos --api 8080
```

Keep this terminal open while using the API.

The server listens on the local loopback interface at port 8080.

Open these URLs in your browser:

| URL | Purpose |
|---|---|
| http://localhost:8080/health | Check API health |
| http://localhost:8080/api/status | View service status |
| http://localhost:8080/api/processes | View process information |
| http://localhost:8080/api/metrics | View process metrics |
| http://localhost:8080/api/anomalies | View detected anomalies |

The health endpoint should return:

```json
{"status":"ok"}
```

The other endpoints return JSON containing the corresponding information.

## 10. Open the Web Dashboard

Keep the API server running.

Open a second Ubuntu terminal and navigate to the project directory:

```bash
cd /mnt/c/Users/idunn/Downloads/OS-Project-member-2
```

Start the dashboard web server:

```bash
python3 -m http.server 8000 --directory dashboard
```

Keep this terminal open.

Open the following address in your browser:

http://localhost:8000

The dashboard provides sections for:

- Overview
- Process Monitor
- Application Supervisor
- Anomaly Detection
- Recovery Center
- Incident Reports
- Settings

The dashboard requests information from the API approximately every three seconds.

**Keep both the API server and dashboard server running while using the dashboard.**

To stop either server, press `Ctrl+C` in its terminal.

## 11. Test Anomaly Detection

This test demonstrates how AegisOS detects sustained high CPU usage.

### Step 1: Start the API

Run:

```bash
./build/aegisos --api 8080
```

If it is already running, leave it running.

### Step 2: Open the dashboard

In a separate terminal, start the dashboard server if it is not already running:

```bash
python3 -m http.server 8000 --directory dashboard
```

Open http://localhost:8000 in your browser.

### Step 3: Create a CPU-intensive process

Open another Ubuntu terminal and run:

```bash
yes > /dev/null &
PID=$!
echo "Test process PID: $PID"
```

This creates a process that consumes CPU resources.

### Step 4: Wait for detection

Keep the dashboard open for approximately 10–15 seconds so that the API receives repeated process requests.

Then open:

http://localhost:8080/api/anomalies

The default anomaly detector uses an 80% CPU threshold and requires three consecutive above-threshold samples.

If the process exceeds the threshold for enough samples, the API should report a `high_cpu` anomaly.

### Step 5: Stop the test process

Run this in the terminal where you started the workload:

```bash
kill "$PID"
```

This test demonstrates detection and reporting. It does not demonstrate automatic recovery from high CPU usage because that workflow is not currently integrated.

## 12. Recovery Policy

AegisOS includes a policy engine and a recovery decision module.

The available recovery modes are:

- `alert_only`: Selects an alert action.
- `dry_run`: Selects a proposed action without executing recovery.
- `auto_restart`: Can select a restart action if the required policy conditions are satisfied.

The policy can consider thresholds, restart limits, cooldown periods, and backoff settings.

The recovery decision module does not itself restart processes. The actual restart behavior currently exists in the separate CLI supervisor.

## 13. Current Limitations

The current implementation has the following limitations:

1. High CPU or memory usage is detected and reported but does not automatically trigger a restart.
2. The CLI supervisor is not directly controlled by the web dashboard.
3. The recovery policy engine makes decisions but does not implement a complete automatic recovery workflow.
4. Anomaly history is stored in memory and is lost when the API server restarts.
5. Some dashboard sections display information rather than providing interactive controls.
6. The dashboard does not provide a complete application registration and management workflow.

These features can be considered future enhancements.

## 14. Project Structure

```text
OS-Project-member-2/
├── include/       Public C header files
├── src/           C source files and main program
├── tests/         Automated tests
├── dashboard/     HTML, CSS, and JavaScript files
├── config/        Example configuration files
├── docs/          Project documentation
├── build/         Compiled executable and test binaries
├── Makefile       Build and test instructions
└── README.md      Project overview and usage guide
```

The `build/` directory is generated by the build process and can be recreated using `make`.

## 15. Troubleshooting

### Error: `No such file or directory`

Make sure you are in the project root and have built the project:

```bash
make
```

### Error: `Address already in use`

Another process may already be using port 8080 or 8000.

Stop the previous server with `Ctrl+C` in its terminal before starting it again.

### Dashboard opens but shows no process data

Check that the API is running:

http://localhost:8080/health

It should return:

```json
{"status":"ok"}
```

Also make sure the dashboard is using the expected API address and port.

### No anomaly appears

Make sure the test workload is still running, process endpoints are being requested repeatedly, and enough time has passed for the required consecutive samples.

### Browser reports `favicon.ico` as missing

A missing favicon is generally harmless and does not prevent the dashboard from functioning.

## 16. Conclusion

AegisOS demonstrates Linux process monitoring, resource anomaly detection, process supervision, restart limits, exponential backoff, policy-based recovery decisions, HTTP API development, and browser-based visualization.

The current implementation can detect sustained resource anomalies and restart supervised programs that exit unsuccessfully. Integrating anomaly detection with automatic policy-controlled recovery remains future work.



## How to Run AegisOS

### 1. Build the Project

Open the Ubuntu terminal in VS Code and run:

```bash
cd /mnt/c/Users/idunn/Downloads/OS-Project-member-2
make clean && make
```

### 2. Run Tests

```bash
make test
```

### 3. Start the API Server

Open Terminal 1 and run:

```bash
./build/aegisos --api 8080
```

Keep this terminal running.

### 4. Start the Dashboard

Open Terminal 2 and run:

```bash
cd /mnt/c/Users/idunn/Downloads/OS-Project-member-2
python3 -m http.server 8000 --directory dashboard
```

Keep this terminal running.

### 5. Open the Dashboard

Open your browser and visit:

http://localhost:8000

The dashboard displays process information and detected anomalies using the AegisOS API.

### 6. Test Process Monitoring

Open Terminal 3 and run:

```bash
cd /mnt/c/Users/idunn/Downloads/OS-Project-member-2
sleep 60 &
PID=$!
./build/aegisos --monitor "$PID" 3
kill "$PID"
```

### 7. Test Process Supervision

Test a successful process:

```bash
./build/aegisos --supervise sh -c 'exit 0'
```

Test a failing process and observe the restart attempts:

```bash
./build/aegisos --supervise sh -c 'exit 1'
```

### 8. Test Anomaly Detection

Run a CPU-intensive process:

```bash
yes > /dev/null &
PID=$!
```

Keep the dashboard open for approximately 10–15 seconds, then visit:

http://localhost:8080/api/anomalies

Stop the test process afterward:

```bash
kill "$PID"
```

### 9. Stop the Servers

Press `Ctrl+C` in the terminals running the API server and dashboard server.

**Note:** Automatic process recovery triggered by anomaly detection is not currently implemented. The recovery module evaluates recovery decisions, while the command-line supervisor independently handles failed child processes.

