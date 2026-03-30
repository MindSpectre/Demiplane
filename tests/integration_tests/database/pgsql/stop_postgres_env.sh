#!/usr/bin/env bash
# Stop PostgreSQL Test Environment

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Detect docker compose command
if docker compose version &> /dev/null; then
    DOCKER_COMPOSE="docker compose"
elif command -v docker-compose &> /dev/null; then
    DOCKER_COMPOSE="docker-compose"
else
    echo "❌ Docker Compose not found"
    exit 1
fi

echo "🛑 Stopping PostgreSQL test database..."
$DOCKER_COMPOSE -f docker-compose.test.yml down

echo "✅ PostgreSQL stopped"
