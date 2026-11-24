# Reading Room — Intelligent Heritage Document Management

A client-oriented prototype for exploring and grouping heritage document records using an original C++17 analysis engine and a multi-page web interface.

## Website pages

The frontend is now split into independent pages with their own URLs and navigation:

- `frontend/index.html` — Overview dashboard
- `frontend/shelves.html` — Topic collections / clusters
- `frontend/explorer.html` — PCA document map, clustering controls and document search
- `frontend/review.html` — DBSCAN human-review queue
- `frontend/analytics.html` — method comparison and diagnostic figures
- `frontend/import.html` — CSV upload and analysis
- `frontend/about.html` — product explanation, limitations and responsible-use guidance

## Requirements

- macOS or Linux with a C++17 compiler (`g++` or `clang++`)
- Python 3 only to serve the static frontend (no Python analysis backend is used)

## Run on macOS

Open Terminal and run:

```bash
cd heritage-cpp/backend
make
./heritage serve data/ndli_sample.csv 5000
```

Keep that terminal open. Open a second Terminal window:

```bash
cd heritage-cpp/frontend
python3 -m http.server 8000
```

Open `http://localhost:8000` in Chrome or Safari. The API runs at `http://127.0.0.1:5000`.

To regenerate the analysis and figures before starting the server:

```bash
cd heritage-cpp/backend
./heritage analyze data/ndli_sample.csv
```

## CSV format

A `title` column is required. Recommended optional columns: `description` (OCR excerpt), `language`, `year`, `pages`, `collection`, and `subject`. The `subject` field is reference metadata only; it is not used to construct clusters.

The current upload endpoint replaces `backend/data/uploaded.csv` and re-runs analysis. Keep a backup of the supplied data before testing client files.

## Important client-readiness notes

This is an enhanced prototype, not a hardened production system. The bundled `ndli_sample.csv` is synthetic demonstration data and must not be represented as an official NDLI export. Before client deployment, validate against a documented real sample and implement authentication, role-based access, audit logging, secure storage, backup/restore, input validation, HTTPS/reverse-proxy deployment, and load testing. PDF ingestion and OCR extraction are not included in this prototype; it currently analyses structured CSV text fields.

The analysis identifies statistical similarities, not authoritative historical subjects. A human archivist must verify groups and all flagged records. DBSCAN noise must never be treated as a reason to delete or suppress a document.
