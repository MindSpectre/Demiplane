set(3RD_PARTY ${DMP_COMMON}.3rdParty)

add_library(${3RD_PARTY}.CsvParser INTERFACE
        csv_parser/csv.hpp
)
target_include_directories(${3RD_PARTY}.CsvParser INTERFACE
        csv_parser
)
add_library(Demiplane::3rdParty::CsvParser ALIAS ${3RD_PARTY}.CsvParser)

add_library(${3RD_PARTY}.ConcurrentQueue INTERFACE
        concurrent_queue/concurrent_queue.hpp
)
target_include_directories(${3RD_PARTY}.ConcurrentQueue INTERFACE
        concurrent_queue
)
add_library(Demiplane::3rdParty::ConcurrentQueue ALIAS ${3RD_PARTY}.ConcurrentQueue)