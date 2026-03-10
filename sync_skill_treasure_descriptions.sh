#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PYTHON_SCRIPT="$SCRIPT_DIR/packages/herokill-core/scripts/sync_skill_treasure_descriptions.py"

DB_HOST="${DB_HOST:-127.0.0.1}"
DB_PORT="${DB_PORT:-3308}"
DB_NAME="${DB_NAME:-hero_kill_boot}"
DB_USER="${DB_USER:-root}"
DB_PASSWORD="${DB_PASSWORD:-123456}"
OUTPUT_DIR="${OUTPUT_DIR:-$SCRIPT_DIR/packages/herokill-core/tmp/db_description_sync}"

usage() {
  cat <<'EOF'
Usage:
  ./sync_skill_treasure_descriptions.sh [options] [-- extra-python-args]

Options:
  --db-host HOST       MySQL host (default: 127.0.0.1)
  --db-port PORT       MySQL port (default: 3308)
  --db-name NAME       Database name (default: hero_kill_boot)
  --db-user USER       Database user (default: root)
  --db-password PASS   Database password (default: 123456)
  --output-dir DIR     Output directory for generated SQL
  -h, --help           Show this help

Examples:
  ./sync_skill_treasure_descriptions.sh
  ./sync_skill_treasure_descriptions.sh -- --scope treasure-items
  ./sync_skill_treasure_descriptions.sh -- --scope all --apply

Environment variables are also supported:
  DB_HOST DB_PORT DB_NAME DB_USER DB_PASSWORD OUTPUT_DIR
EOF
}

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Error: required command not found: $1" >&2
    exit 1
  fi
}

EXTRA_ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --db-host) DB_HOST="$2"; shift 2 ;;
    --db-port) DB_PORT="$2"; shift 2 ;;
    --db-name) DB_NAME="$2"; shift 2 ;;
    --db-user) DB_USER="$2"; shift 2 ;;
    --db-password) DB_PASSWORD="$2"; shift 2 ;;
    --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    --)
      shift
      EXTRA_ARGS=("$@")
      break
      ;;
    *)
      echo "Error: unknown argument: $1" >&2
      usage
      exit 1
      ;;
  esac
done

require_cmd python3

if [[ ! -f "$PYTHON_SCRIPT" ]]; then
  echo "Error: python sync script not found: $PYTHON_SCRIPT" >&2
  exit 1
fi

export PYTHONPYCACHEPREFIX="${PYTHONPYCACHEPREFIX:-/tmp/python-pyc-cache}"

exec python3 "$PYTHON_SCRIPT" \
  --db-host "$DB_HOST" \
  --db-port "$DB_PORT" \
  --db-name "$DB_NAME" \
  --db-user "$DB_USER" \
  --db-password "$DB_PASSWORD" \
  --output-dir "$OUTPUT_DIR" \
  "${EXTRA_ARGS[@]}"
