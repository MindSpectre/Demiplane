add_library(${PROJECT_NAME}_3rdParty_CsvParser INTERFACE csv_parser/csv.h)
target_include_directories(${PROJECT_NAME}_3rdParty_CsvParser INTERFACE csv_parser)
add_library(3rdParty::CsvParser ALIAS ${PROJECT_NAME}_3rdParty_CsvParser)