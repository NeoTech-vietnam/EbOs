# Module-owned source and public include metadata.
list(APPEND NEO_MODULE_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Os_EspIdf/EbOs_EspIdf.c"
    "${CMAKE_CURRENT_LIST_DIR}/Os_EspIdf/Os_EspIdf.c"
)
list(APPEND NEO_MODULE_INCLUDES
    "${CMAKE_CURRENT_LIST_DIR}/Os"
    "${CMAKE_CURRENT_LIST_DIR}/OsShell"
)
