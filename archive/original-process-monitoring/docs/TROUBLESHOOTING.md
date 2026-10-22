# Troubleshooting

## Build
- `make: cc: not found` → `sudo apt install build-essential`.
- `*** missing separator` in Makefile → recipes must be indented with a tab.
- Slow builds/IO on WSL2 → keep the project under `~`, not `/mnt/c/...`.

## VS Code
- No IntelliSense for headers → install the C/C++ extension; `.vscode/c_cpp_properties.json` adds `include/`.
- Debugger cannot start → `sudo apt install gdb`; run the `build` task first.

## Runtime (to expand as features land)
- Permission denied reading `/proc/<pid>` → process owned by another user.
- Port already in use → _(add after the API exists)_
