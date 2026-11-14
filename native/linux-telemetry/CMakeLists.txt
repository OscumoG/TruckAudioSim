set(scs_dir "${PROJECT_SOURCE_DIR}/third_party/scs-sdk-plugin")
if(NOT EXISTS "${scs_dir}/scs_sdk/include/scssdk.h")
  message(FATAL_ERROR "Initialize the SDK with git submodule update --init")
endif()
add_library(tas_telemetry_linux MODULE telemetry.cpp)
target_include_directories(tas_telemetry_linux PRIVATE "${scs_dir}/scs_sdk/include" "${scs_dir}/scs-telemetry/inc")
target_compile_options(tas_telemetry_linux PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(tas_telemetry_linux PROPERTIES PREFIX "" OUTPUT_NAME "trucker_aux_telemetry"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins" CXX_VISIBILITY_PRESET hidden)
# The game can run inside the Steam Linux Runtime with an older libstdc++.
target_link_options(tas_telemetry_linux PRIVATE -static-libstdc++ -static-libgcc)
