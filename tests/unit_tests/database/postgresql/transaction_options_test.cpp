#include <gtest/gtest.h>
#include <options/transaction_options.hpp>

using namespace demiplane::db::postgres;

class TransactionOptionsTest : public ::testing::Test {};

TEST_F(TransactionOptionsTest, DefaultOptionsProduceReadCommittedReadWrite) {
    TransactionOptions opts;
    EXPECT_EQ(opts.to_begin_sql(), "BEGIN ISOLATION LEVEL READ COMMITTED READ WRITE");
}

TEST_F(TransactionOptionsTest, RepeatableReadIsolation) {
    TransactionOptions opts{.isolation = IsolationLevel::REPEATABLE_READ};
    EXPECT_EQ(opts.to_begin_sql(), "BEGIN ISOLATION LEVEL REPEATABLE READ READ WRITE");
}

TEST_F(TransactionOptionsTest, SerializableReadOnly) {
    TransactionOptions opts{
        .isolation = IsolationLevel::SERIALIZABLE,
        .access    = AccessMode::READ_ONLY,
    };
    EXPECT_EQ(opts.to_begin_sql(), "BEGIN ISOLATION LEVEL SERIALIZABLE READ ONLY");
}

TEST_F(TransactionOptionsTest, SerializableReadOnlyDeferrable) {
    TransactionOptions opts{
        .isolation  = IsolationLevel::SERIALIZABLE,
        .access     = AccessMode::READ_ONLY,
        .deferrable = true,
    };
    EXPECT_EQ(opts.to_begin_sql(), "BEGIN ISOLATION LEVEL SERIALIZABLE READ ONLY DEFERRABLE");
}

TEST_F(TransactionOptionsTest, DeferrableWithNonSerializableThrows) {
    TransactionOptions opts{
        .isolation  = IsolationLevel::READ_COMMITTED,
        .deferrable = true,
    };
    EXPECT_THROW(static_cast<void>(opts.to_begin_sql()), std::invalid_argument);
}

TEST_F(TransactionOptionsTest, DeferrableWithSerializableReadWriteThrows) {
    TransactionOptions opts{
        .isolation  = IsolationLevel::SERIALIZABLE,
        .access     = AccessMode::READ_WRITE,
        .deferrable = true,
    };
    EXPECT_THROW(static_cast<void>(opts.to_begin_sql()), std::invalid_argument);
}
