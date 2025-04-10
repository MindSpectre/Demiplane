#pragma once

#include <iostream>
#include <memory>

#include "basic_mock_db_client.hpp"
#include "db_interface.hpp"
#include "pqxx_client.hpp"
#include "silent_mock_db_client.hpp"
#include "db_exceptions.hpp"

namespace demiplane::database::creational {
    template <class Service, typename FactoryFunc, typename... Args>
    concept DatabaseFactoryFunction =
        std::invocable<FactoryFunc, Args...>
        && std::same_as<std::invoke_result_t<FactoryFunc, Args...>, std::unique_ptr<Service>>;

    class DatabaseFactory {
    public:
        static std::unique_ptr<PqxxClient> create_pqxx_client(const ConnectParams& _params) {
            std::unique_ptr<PqxxClient> connect;
            try {
                connect = std::make_unique<PqxxClient>(_params);
            } catch (const exceptions::ConnectionException& e) {
                std::cerr << "Cannot connect to database. Trying to resolve problem and create DB...." << std::endl;
                std::cerr << e.what() << std::endl;

                try {
                    connect->create_database({}, _params);
                    connect = std::make_unique<PqxxClient>(_params);
                } catch (const std::exception& inner_e) {
                    std::string msg = "Failed to open database connection. Cascade of fails.";
                    msg.append(inner_e.what());
                    throw exceptions::ConnectionException(msg, errors::db_error_code::CONNECTION_FAILED);
                }
            }
            return connect;
        }

        static std::unique_ptr<BasicMockDbClient> create_basic_mock_database_prm(const ConnectParams& _params) {

            return std::make_unique<BasicMockDbClient>(_params);
        }
        static std::unique_ptr<BasicMockDbClient> create_basic_mock_database() {
            return std::make_unique<BasicMockDbClient>();
        }
        static std::unique_ptr<SilentMockDbClient> create_silent_mock_database() {
            return std::make_unique<SilentMockDbClient>();
        }
    };


} // namespace demiplane::database::creational
