set(3RD_PARTY 3rdParty)

add_library(${PROJECT_NAME}.${3RD_PARTY}.CsvParser INTERFACE 
        csv_parser/csv.h
)
target_include_directories(${PROJECT_NAME}.${3RD_PARTY}.CsvParser INTERFACE 
        csv_parser
)
add_library(Demiplane::3rdParty::CsvParser ALIAS ${PROJECT_NAME}.${3RD_PARTY}.CsvParser)