#!/usr/bin/env bash

set -euo pipefail

LOCAL_HOST="${LOCAL_HOST:-127.0.0.1}"
LOCAL_PORT="${LOCAL_PORT:-3308}"
LOCAL_USER="${LOCAL_USER:-root}"
LOCAL_PASS="${LOCAL_PASS:-123456}"

REMOTE_HOST="${REMOTE_HOST:-h.v90.store}"
REMOTE_PORT="${REMOTE_PORT:-3308}"
REMOTE_USER="${REMOTE_USER:-root}"
REMOTE_PASS="${REMOTE_PASS:-123456}"

DB_NAME="${DB_NAME:-hero_kill_boot}"
ASSUME_YES=false

usage() {
  cat <<'EOF'
Usage:
  ./sync_local_db_to_remote.sh [options]

Options:
  --db NAME             Database name (default: hero_kill_boot)
  --local-host HOST     Local MySQL host (default: 127.0.0.1)
  --local-port PORT     Local MySQL port (default: 3308)
  --local-user USER     Local MySQL user (default: root)
  --local-pass PASS     Local MySQL password (default: 123456)
  --remote-host HOST    Remote MySQL host (default: h.v90.store)
  --remote-port PORT    Remote MySQL port (default: 3308)
  --remote-user USER    Remote MySQL user (default: root)
  --remote-pass PASS    Remote MySQL password (default: 123456)
  --yes                 Skip interactive confirmation
  -h, --help            Show this help

Env vars are also supported:
  LOCAL_HOST LOCAL_PORT LOCAL_USER LOCAL_PASS
  REMOTE_HOST REMOTE_PORT REMOTE_USER REMOTE_PASS
  DB_NAME
EOF
}

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Error: required command not found: $1" >&2
    exit 1
  fi
}

validate_db_name() {
  if [[ ! "$1" =~ ^[A-Za-z0-9_]+$ ]]; then
    echo "Error: invalid database name '$1'. Allowed chars: A-Z a-z 0-9 _" >&2
    exit 1
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --db) DB_NAME="$2"; shift 2 ;;
    --local-host) LOCAL_HOST="$2"; shift 2 ;;
    --local-port) LOCAL_PORT="$2"; shift 2 ;;
    --local-user) LOCAL_USER="$2"; shift 2 ;;
    --local-pass) LOCAL_PASS="$2"; shift 2 ;;
    --remote-host) REMOTE_HOST="$2"; shift 2 ;;
    --remote-port) REMOTE_PORT="$2"; shift 2 ;;
    --remote-user) REMOTE_USER="$2"; shift 2 ;;
    --remote-pass) REMOTE_PASS="$2"; shift 2 ;;
    --yes) ASSUME_YES=true; shift ;;
    -h|--help) usage; exit 0 ;;
    *)
      echo "Error: unknown argument: $1" >&2
      usage
      exit 1
      ;;
  esac
done

require_cmd mysql
require_cmd mysqldump
validate_db_name "$DB_NAME"

LOCAL_MYSQL_ARGS=(
  --protocol=TCP
  -h"$LOCAL_HOST"
  -P"$LOCAL_PORT"
  -u"$LOCAL_USER"
  --default-character-set=utf8mb4
)

REMOTE_MYSQL_ARGS=(
  --protocol=TCP
  -h"$REMOTE_HOST"
  -P"$REMOTE_PORT"
  -u"$REMOTE_USER"
  --default-character-set=utf8mb4
)

run_local_mysql() {
  MYSQL_PWD="$LOCAL_PASS" mysql "${LOCAL_MYSQL_ARGS[@]}" "$@"
}

run_remote_mysql() {
  MYSQL_PWD="$REMOTE_PASS" mysql "${REMOTE_MYSQL_ARGS[@]}" "$@"
}

run_local_dump() {
  MYSQL_PWD="$LOCAL_PASS" mysqldump "${LOCAL_MYSQL_ARGS[@]}" \
    --single-transaction \
    --quick \
    --routines \
    --events \
    --triggers \
    --hex-blob \
    --set-gtid-purged=OFF \
    "$DB_NAME"
}

echo "[1/5] Check local MySQL connection: ${LOCAL_HOST}:${LOCAL_PORT}"
run_local_mysql -Nse "SELECT 1;" >/dev/null

echo "[2/5] Check remote MySQL connection: ${REMOTE_HOST}:${REMOTE_PORT}"
run_remote_mysql -Nse "SELECT 1;" >/dev/null

if ! run_local_mysql -Nse "SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='${DB_NAME}'" | grep -q "^${DB_NAME}$"; then
  echo "Error: local database not found: ${DB_NAME}" >&2
  exit 1
fi

echo "[3/5] Ready to replace remote database '${DB_NAME}' on ${REMOTE_HOST}:${REMOTE_PORT}"
if ! $ASSUME_YES; then
  echo "WARNING: this will DROP and recreate remote database '${DB_NAME}'."
  read -r -p "Type YES to continue: " confirm
  if [[ "$confirm" != "YES" ]]; then
    echo "Cancelled."
    exit 1
  fi
fi

echo "[4/5] Recreate remote database"
run_remote_mysql -e "DROP DATABASE IF EXISTS \`${DB_NAME}\`; CREATE DATABASE \`${DB_NAME}\` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"

echo "[5/5] Dump local and import to remote (stream mode)"
run_local_dump | MYSQL_PWD="$REMOTE_PASS" mysql "${REMOTE_MYSQL_ARGS[@]}" "$DB_NAME"

echo "Done: ${DB_NAME} synced from ${LOCAL_HOST}:${LOCAL_PORT} to ${REMOTE_HOST}:${REMOTE_PORT}"
