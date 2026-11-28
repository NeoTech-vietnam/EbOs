if(CMAKE_BUILD_EARLY_EXPANSION)
    # IDF discovers requirements in a separate scope before project variables.
    idf_component_register(PRIV_REQUIRES freertos heap esp_timer)
    return()
endif()

if(NOT EXISTS "${NEO_INTEGRATION_DIR}/NeoContracts/Neo_Types.h" OR
   NOT EXISTS "${NEO_INTEGRATION_DIR}/CUCEL/Standard/Std_Types.h")
    message(FATAL_ERROR "Assemble a compatible workspace and set NEO_INTEGRATION_DIR")
endif()
idf_component_register(
    SRCS "main.c" "${NEO_EBOS_DIR}/Os_EspIdf/EbOs_EspIdf.c"
    INCLUDE_DIRS "${NEO_EBOS_DIR}/OsShell" "${NEO_INTEGRATION_DIR}/NeoContracts"
        "${NEO_INTEGRATION_DIR}/CUCEL/Standard" "${NEO_INTEGRATION_DIR}/CUCEL/Platform"
    PRIV_REQUIRES freertos heap esp_timer
)
