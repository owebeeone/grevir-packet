if(NOT COMMAND catch_discover_tests)
  find_package(grevir-test-support CONFIG REQUIRED)
endif()
add_executable(grevir_packet_runtime packet_reassembler_test.cpp)
target_link_libraries(grevir_packet_runtime PRIVATE grevir::packet Catch2::Catch2WithMain)
set_target_properties(grevir_packet_runtime PROPERTIES CXX_EXTENSIONS OFF)
catch_discover_tests(grevir_packet_runtime TEST_PREFIX "packet."
  PROPERTIES LABELS "packet" TIMEOUT 10)
