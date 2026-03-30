#!/usr/bin/env bash
# Environment variables for PostgreSQL test database
# Source this file: source tests/scripts/db-test-env.sh

export POSTGRES_HOST=localhost
export POSTGRES_PORT=5433
export POSTGRES_DB=test_db
export POSTGRES_USER=test_user
export POSTGRES_PASSWORD=test_password

echo "✅ PostgreSQL test environment variables set:"
echo "   POSTGRES_HOST=$POSTGRES_HOST"
echo "   POSTGRES_PORT=$POSTGRES_PORT"
echo "   POSTGRES_DB=$POSTGRES_DB"
echo "   POSTGRES_USER=$POSTGRES_USER"
echo "   POSTGRES_PASSWORD=***"
