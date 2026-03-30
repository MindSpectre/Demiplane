#!/usr/bin/env bash
# Start PostgreSQL test database container

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
COMPOSE_FILE="$PROJECT_ROOT/tests/docker-compose.test.yml"

# Detect docker compose command (v2 uses 'docker compose', v1 uses 'docker-compose')
if docker compose version &> /dev/null; then
    DOCKER_COMPOSE="docker compose"
elif command -v docker-compose &> /dev/null; then
    DOCKER_COMPOSE="docker-compose"
else
    echo "❌ Neither 'docker compose' nor 'docker-compose' found"
    echo "Please install Docker Compose: https://docs.docker.com/compose/install/"
    exit 1
fi

echo "🐳 Starting PostgreSQL test database..."

# Start the container
$DOCKER_COMPOSE -f "$COMPOSE_FILE" up -d

echo "⏳ Waiting for PostgreSQL to be ready..."

# Wait for PostgreSQL to be healthy
timeout=30
elapsed=0
until $DOCKER_COMPOSE -f "$COMPOSE_FILE" exec -T postgres-test pg_isready -U test_user -d test_db > /dev/null 2>&1; do
    if [ $elapsed -ge $timeout ]; then
        echo "❌ PostgreSQL failed to start within ${timeout}s"
        echo "Container logs:"
        $DOCKER_COMPOSE -f "$COMPOSE_FILE" logs postgres-test
        exit 1
    fi
    sleep 1
    elapsed=$((elapsed + 1))
    echo -n "."
done

echo ""
echo "✅ PostgreSQL test database is ready!"
echo ""
echo "Connection details:"
echo "  Host:     localhost"
echo "  Port:     5433"
echo "  Database: test_db"
echo "  User:     test_user"
echo "  Password: test_password"
echo ""
echo "Environment variables for tests:"
echo "  export POSTGRES_HOST=localhost"
echo "  export POSTGRES_PORT=5433"
echo "  export POSTGRES_DB=test_db"
echo "  export POSTGRES_USER=test_user"
echo "  export POSTGRES_PASSWORD=test_password"
echo ""
echo "Or source the env file:"
echo "  source $PROJECT_ROOT/tests/scripts/db-test-env.sh"
