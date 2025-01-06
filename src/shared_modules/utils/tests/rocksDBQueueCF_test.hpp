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

#ifndef _ROCKSDB_QUEUE_CF_TEST_HPP
#define _ROCKSDB_QUEUE_CF_TEST_HPP

#include "rocksDBQueueCF.hpp"
#include <gtest/gtest.h>
#include <memory>

constexpr auto TEST_DB = "test_CF.db";

class RocksDBQueueCFTest : public ::testing::Test
{
protected:
    RocksDBQueueCFTest() = default;
    ~RocksDBQueueCFTest() override = default;
    std::unique_ptr<RocksDBQueueCF<std::string>> queue;
    void SetUp() override;
    void TearDown() override;
};
#endif //_ROCKSDB_QUEUE_CF_TEST_HPP
