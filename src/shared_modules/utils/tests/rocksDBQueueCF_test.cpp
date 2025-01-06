/*
 * Wazuh shared modules utils
 * Copyright (C) 2015, Wazuh Inc.
 * Jan 06, 2025.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include "rocksDBQueueCF_test.hpp"
#include <filesystem>

void RocksDBQueueCFTest::SetUp()
{
    std::error_code ec;
    std::filesystem::remove_all(TEST_DB, ec);
    queue = std::make_unique<RocksDBQueueCF<std::string>>(TEST_DB);
};

void RocksDBQueueCFTest::TearDown() {};

// Test pushing elements and validating size and non-emptiness of the queue
TEST_F(RocksDBQueueCFTest, PushIncreasesSizeAndNonEmptyState)
{
    // Push elements into the queue
    queue->push("001", "first");
    queue->push("001", "second");
    queue->push("002", "third");

    // Verify the size of the queue
    EXPECT_EQ(queue->size("001"), 2);
    EXPECT_EQ(queue->size("002"), 1);

    // Verify the queue is not empty
    EXPECT_FALSE(queue->empty());
}

// Test correct key padding for RocksDB
TEST_F(RocksDBQueueCFTest, KeyPaddingIsCorrect)
{
    // Push elements into the queue
    queue->push("001", "value1");
    queue->push("002", "value2");

    // Open RocksDB in read-only mode to verify keys
    rocksdb::DB* db;
    rocksdb::Options options;
    options.create_if_missing = true;
    rocksdb::Status status = rocksdb::DB::OpenForReadOnly(options, TEST_DB, &db);

    ASSERT_TRUE(status.ok()) << "Failed to open database in read-only mode: " << status.ToString();

    {
        // Use iterator to verify keys
        auto it = std::unique_ptr<rocksdb::Iterator>(db->NewIterator(rocksdb::ReadOptions()));

        // Validate the first key and its value
        it->SeekToFirst();
        ASSERT_TRUE(it->Valid());
        EXPECT_EQ(it->key().ToString(), "0000000001_0000000001");
        EXPECT_EQ(it->value().ToString(), "value1");

        // Validate the second key and its value
        it->Next();
        ASSERT_TRUE(it->Valid());
        EXPECT_EQ(it->key().ToString(), "0000000002_0000000001");
        EXPECT_EQ(it->value().ToString(), "value2");

        // Ensure no more keys exist
        it->Next();
        EXPECT_FALSE(it->Valid());
    }

    // Clean up RocksDB instance
    delete db;
}

// Test correct key padding for RocksDB with pre-existent keys not padded
TEST_F(RocksDBQueueCFTest, KeyPaddingIsCorrectPreExistentKeysNotPadded)
{
    // Load pre-existent keys into the database
    queue.reset();
    rocksdb::DB* db;
    rocksdb::Options options;
    options.create_if_missing = true;
    rocksdb::Status status = rocksdb::DB::Open(options, TEST_DB, &db);
    ASSERT_TRUE(status.ok()) << "Failed to open database: " << status.ToString();

    std::string binaryValue = {'\xA1', '\x3A', '\x5F', '\x00', '\x10', '\xDA', '\x0F', '\x1A'};

    db->Put(rocksdb::WriteOptions(), "1_1", "value1");
    db->Put(rocksdb::WriteOptions(), "1_2", "value2");
    db->Put(rocksdb::WriteOptions(), "1_3", binaryValue);
    delete db;

    // Retrieve the values
    queue = std::make_unique<RocksDBQueueCF<std::string>>(TEST_DB);

    EXPECT_EQ(queue->size("001"), 3);

    auto value = queue->front("001");
    EXPECT_EQ(value, "value1");
    queue->pop("001");

    value = queue->front("001");
    EXPECT_EQ(value, "value2");
    queue->pop("001");

    value = queue->front("001");
    EXPECT_EQ(value, binaryValue);
    queue->pop("001");
}

// Test popping an element updates the queue correctly
TEST_F(RocksDBQueueCFTest, PopMethodRemovesFirstElement)
{
    // Push elements into the queue
    queue->push("001", "value1");
    queue->push("001", "value2");

    // Pop the first element
    queue->pop("001");

    // Open RocksDB in read-only mode to verify keys
    rocksdb::DB* db;
    rocksdb::Options options;
    options.create_if_missing = true;
    rocksdb::Status status = rocksdb::DB::OpenForReadOnly(options, TEST_DB, &db);

    ASSERT_TRUE(status.ok()) << "Failed to open database in read-only mode: " << status.ToString();

    {
        // Use iterator to verify keys
        auto it = std::unique_ptr<rocksdb::Iterator>(db->NewIterator(rocksdb::ReadOptions()));

        // Validate the first remaining key and its value
        it->SeekToFirst();
        ASSERT_TRUE(it->Valid());
        EXPECT_EQ(it->key().ToString(), "0000000001_0000000002");
        EXPECT_EQ(it->value().ToString(), "value2");

        // Ensure no more keys exist
        it->Next();
        EXPECT_FALSE(it->Valid());
    }

    // Clean up RocksDB instance
    delete db;
}

// Test retrieving the front element of the queue
TEST_F(RocksDBQueueCFTest, FrontMethodReturnsFirstElement)
{
    // Push elements into the queue
    queue->push("001", "value1");
    queue->push("001", "value2");

    // Retrieve the front element
    auto value = queue->front("001");

    // Verify the value of the front element
    EXPECT_EQ(value, "value1");
}
