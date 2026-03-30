#!/usr/bin/env bash
# Run functional tests with PostgreSQL test database

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Source environment variables
source "$SCRIPT_DIR/db-test-env.sh"

echo ""
echo "🧪 Running functional tests..."
echo ""

cd "$PROJECT_ROOT"

# Check if build directory exists
if [ ! -d "build/debug" ]; then
    echo "❌ Build directory not found. Please run cmake --preset debug first."
    exit 1
fi

# Run functional tests with ctest
ctest --preset debug -L functional --output-on-failure "$@"

TEST_EXIT_CODE=$?

if [ $TEST_EXIT_CODE -eq 0 ]; then
    echo ""
    echo "✅ All functional tests passed!"
else
    echo ""
    echo "❌ Some functional tests failed (exit code: $TEST_EXIT_CODE)"
fi

exit $TEST_EXIT_CODE
