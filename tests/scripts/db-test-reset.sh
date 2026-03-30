#!/usr/bin/env bash
# Reset PostgreSQL test database (destroy and recreate)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
COMPOSE_FILE="$PROJECT_ROOT/tests/docker-compose.test.yml"

# Detect docker compose command
if docker compose version &> /dev/null; then
    DOCKER_COMPOSE="docker compose"
elif command -v docker-compose &> /dev/null; then
    DOCKER_COMPOSE="docker-compose"
else
    echo "❌ Neither 'docker compose' nor 'docker-compose' found"
    exit 1
fi

echo "🔄 Resetting PostgreSQL test database..."

# Stop and remove containers and volumes
$DOCKER_COMPOSE -f "$COMPOSE_FILE" down -v

echo "✅ Database reset complete"
echo ""
echo "To start the database again, run:"
echo "  $SCRIPT_DIR/db-test-start.sh"
