/*
 * Wazuh - Indexer connector.
 * Copyright (C) 2015, Wazuh Inc.
 * June 2, 2023.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include "indexerConnector/indexerConnector.hpp"
#include "HTTPRequest.hpp"
#include "base/logging.hpp"
#include "base/utils/stringUtils.hpp"
#include "base/utils/timeUtils.hpp"
#include "secureCommunication.hpp"
#include "serverSelector.hpp"
#include <fstream>

constexpr auto INDEXER_COLUMN {"indexer"};
constexpr auto USER_KEY {"username"};
constexpr auto PASSWORD_KEY {"password"};
constexpr auto ELEMENTS_PER_BULK {1000};

// Single thread in case the events needs to be processed in order.
constexpr auto SINGLE_ORDERED_DISPATCHING = 1;

static void initConfiguration(SecureCommunication& secureCommunication, const IndexerConnectorOptions& config)
{
    std::string caRootCertificate;
    std::string sslCertificate;
    std::string sslKey;
    std::string username;
    std::string password;

    if (!config.sslOptions.cacert.empty())
    {
        caRootCertificate = config.sslOptions.cacert.front();
    }

    sslCertificate = config.sslOptions.cert;
    sslKey = config.sslOptions.key;
    username = config.username;
    password = config.password;

    if (config.username.empty())
    {
        username = "admin";
        LOG_WARNING("No username found in the configuration, using default value.");
    }

    if (config.password.empty())
    {
        password = "admin";
        LOG_WARNING("No password found in the configuration, using default value.");
    }

    secureCommunication.basicAuth(username + ":" + password)
        .sslCertificate(sslCertificate)
        .sslKey(sslKey)
        .caRootCertificate(caRootCertificate);
}

static void builderBulkDelete(std::string& bulkData, std::string_view id, std::string_view index)
{
    bulkData.append(R"({"delete":{"_index":")");
    bulkData.append(index);
    bulkData.append(R"(","_id":")");
    bulkData.append(id);
    bulkData.append(R"("}})");
    bulkData.append("\n");
}

static void builderBulkIndex(std::string& bulkData, std::string_view id, std::string_view index, std::string_view data)
{
    bulkData.append(R"({"index":{"_index":")");
    bulkData.append(index);

    if (!id.empty())
    {
        bulkData.append(R"(","_id":")");
        bulkData.append(id);
    }

    bulkData.append(R"("}})");
    bulkData.append("\n");
    bulkData.append(data);
    bulkData.append("\n");
}

IndexerConnector::IndexerConnector(const IndexerConnectorOptions& indexerConnectorOptions)
{
    // Get index name.
    m_indexName = indexerConnectorOptions.name;

    base::utils::string::replaceAll(m_indexName, "$(date)", base::utils::time::getCurrentDate("."));

    if (base::utils::string::haveUpperCaseCharacters(m_indexName))
    {
        throw std::invalid_argument("Index name must be lowercase.");
    }

    auto secureCommunication = SecureCommunication::builder();
    initConfiguration(secureCommunication, indexerConnectorOptions);

    // Initialize publisher.
    auto selector {std::make_shared<TServerSelector<Monitoring>>(
        indexerConnectorOptions.hosts, indexerConnectorOptions.timeout, secureCommunication)};

    // Validate threads number
    if (indexerConnectorOptions.workingThreads <= 0)
    {
        LOG_DEBUG("Invalid number of working threads, using default value.");
    }

    m_dispatcher = std::make_unique<ThreadDispatchQueue>(
        [this, selector, secureCommunication, functionName = logging::getLambdaName(__FUNCTION__, "processEventQueue")](
            std::queue<std::string>& dataQueue)
        {
            std::scoped_lock lock(m_syncMutex);

            if (m_stopping.load())
            {
                LOG_DEBUG_L(functionName.c_str(), "IndexerConnector is stopping, event processing will be skipped.");
                throw std::runtime_error("IndexerConnector is stopping, event processing will be skipped.");
            }

            auto url = selector->getNext();
            std::string bulkData;
            url.append("/_bulk?refresh=wait_for");

            while (!dataQueue.empty())
            {
                auto data = dataQueue.front();
                dataQueue.pop();
                auto parsedData = nlohmann::json::parse(data, nullptr, false);

                if (parsedData.is_discarded())
                {
                    continue;
                }

                if (parsedData.at("operation").get_ref<const std::string&>().compare("DELETED") == 0)
                {
                    const auto& id = parsedData.at("id").get_ref<const std::string&>();
                    builderBulkDelete(bulkData, id, m_indexName);
                }
                else
                {
                    const auto& id = parsedData.contains("id") ? parsedData.at("id").get_ref<const std::string&>() : "";
                    const auto dataString = parsedData.at("data").dump();
                    builderBulkIndex(bulkData, id, m_indexName, dataString);
                }
            }

            if (!bulkData.empty())
            {
                // Process data.
                HTTPRequest::instance().post(
                    {HttpURL(url), bulkData, secureCommunication},
                    {[functionName = logging::getLambdaName(__FUNCTION__, "handleSuccessfulPostResponse")](
                         const std::string& response)
                     { LOG_DEBUG_L(functionName.c_str(), "Response: %s", response.c_str()); },
                     [functionName = logging::getLambdaName(__FUNCTION__, "handlePostResponseError")](
                         const std::string& error, const long statusCode)
                     {
                         LOG_ERROR_L(functionName.c_str(), "%s, status code: %ld.", error.c_str(), statusCode);
                         throw std::runtime_error(error);
                     }});
            }
        },
        indexerConnectorOptions.databasePath + m_indexName,
        ELEMENTS_PER_BULK,
        indexerConnectorOptions.workingThreads <= 0 ? SINGLE_ORDERED_DISPATCHING
                                                    : indexerConnectorOptions.workingThreads);
}

IndexerConnector::~IndexerConnector()
{
    m_stopping.store(true);
    m_cv.notify_all();

    m_dispatcher->cancel();
}

void IndexerConnector::publish(const std::string& message)
{
    m_dispatcher->push(message);
}
