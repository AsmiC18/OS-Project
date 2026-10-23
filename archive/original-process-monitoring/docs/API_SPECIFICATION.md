# API Specification (draft)

> Status: **nothing here is implemented.** Routes and shapes may change.

- Base URL: `http://127.0.0.1:<port>` (loopback only by default; port TBD, configurable).
- Content type: `application/json`. Errors: `{ "error": "<code>", "message": "<text>" }` with an appropriate HTTP status.
- No route accepts or runs arbitrary shell commands. Action routes only reference **registered application IDs**.
- All inputs validated (types, ranges, length); unknown routes → 404; wrong method → 405.

## Read routes (planned)
| Route | Purpose |
|---|---|
| `GET /api/metrics` | System-level CPU/memory summary and timestamp |
| `GET /api/processes` | Samples for monitored processes |
| `GET /api/applications` | Registered apps: state, PID, restart count, uptime/downtime |
| `GET /api/incidents` | Incident history; filters: `severity`, `app`, `from`, `to`, `limit` |
| `GET /api/reports` | Summary statistics (detection delay, restart success rate, recovery time) |
| `GET /api/reports/export.csv` | CSV export (name TBD) |

## Action routes (planned, authorized apps only)
| Route | Purpose |
|---|---|
| `POST /api/applications/{id}/start`, `/stop`, `/restart` | Lifecycle control |
| `POST /api/incidents/{id}/approve` | Approve a pending recovery action |
| `PUT /api/settings` | Change supported settings (e.g. sample interval) |

## To define
JSON schemas, timestamp format (proposal: UTC ISO-8601 + epoch ms), pagination, CORS stance (same-origin serving of `dashboard/` preferred), request-size limits.
