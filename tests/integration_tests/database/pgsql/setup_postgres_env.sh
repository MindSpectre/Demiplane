#!/usr/bin/env bash
# PostgreSQL Test Environment Setup
# This script starts PostgreSQL Docker container and exports environment variables

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Load environment from .env.test.local or .env.test
ENV_FILE=".env.test.local"
if [ ! -f "$ENV_FILE" ]; then
    ENV_FILE=".env.test"
fi

if [ -f "$ENV_FILE" ]; then
    set -a
    # shellcheck disable=SC1090
    source <(grep -v '^#' "$ENV_FILE" | grep -v '^$')
    set +a
fi

# Detect docker compose command
if docker compose version &> /dev/null; then
    DOCKER_COMPOSE="docker compose"
elif command -v docker-compose &> /dev/null; then
    DOCKER_COMPOSE="docker-compose"
else
    echo "❌ Docker Compose not found"
    exit 1
fi

echo "🐳 Starting PostgreSQL test database..."
$DOCKER_COMPOSE -f docker-compose.test.yml up -d

echo "⏳ Waiting for PostgreSQL to be ready..."
for _ in {1..30}; do
    if $DOCKER_COMPOSE -f docker-compose.test.yml exec -T postgres-test pg_isready -U "$POSTGRES_USER" -d "$POSTGRES_DB" &> /dev/null; then
        echo "✅ PostgreSQL is ready!"
        echo ""
        echo "Connection: postgresql://$POSTGRES_USER:***@$POSTGRES_HOST:$POSTGRES_PORT/$POSTGRES_DB"
        exit 0
    fi
    sleep 1
done

echo "❌ PostgreSQL failed to start"
$DOCKER_COMPOSE -f docker-compose.test.yml logs
exit 1
