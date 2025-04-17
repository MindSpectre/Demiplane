#pragma once
// TODO: Create class accepting File + ENV or file(encrypted) or runtime pwd to configuring PQXX client

namespace demiplane::database {
    class PostgresConfig {
    // MOCKED
    public:
        bool use_trgm = true;
        bool use_fts = true;
    };

    class PostgresConfigurator {

    };


}