set(3RD_PARTY ${DMP_COMMON}.3rdParty)

##############################################################################
# Csv parser
##############################################################################
message("Using rapidcsv parser. HEAD https://github.com/d99kris/rapidcsv")
add_library(${3RD_PARTY}.CsvParser INTERFACE
        csv_parser/csv.hpp
)
target_include_directories(${3RD_PARTY}.CsvParser INTERFACE
        csv_parser
)
add_library(Demiplane::3rdParty::CsvParser ALIAS ${3RD_PARTY}.CsvParser)
##############################################################################

##############################################################################
#  Moodycamel concurrent queue
##############################################################################
message("Using moodycamel concurrent queue. HEAD https://github.com/cameron314/concurrentqueue")
add_library(${3RD_PARTY}.ConcurrentQueue INTERFACE
        concurrent_queue/concurrent_queue.hpp
)
target_include_directories(${3RD_PARTY}.ConcurrentQueue INTERFACE
        concurrent_queue
)
add_library(Demiplane::3rdParty::ConcurrentQueue ALIAS ${3RD_PARTY}.ConcurrentQueue)
##############################################################################

