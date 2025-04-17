set(3RD_PARTY ${DMP_CORE}.3rdParty)

add_library(${3RD_PARTY}.CsvParser INTERFACE
        csv_parser/csv.h
)
target_include_directories(${3RD_PARTY}.CsvParser INTERFACE
        csv_parser
)
add_library(Demiplane::3rdParty::CsvParser ALIAS ${3RD_PARTY}.CsvParser)