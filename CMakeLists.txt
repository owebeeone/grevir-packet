cmake_minimum_required(VERSION 3.20)
project(grevir_packet VERSION 0.1.0 LANGUAGES CXX)

if(NOT TARGET grevir::base)
  find_package(grevir-base CONFIG REQUIRED)
endif()

add_library(grevir_packet INTERFACE)
add_library(grevir::packet ALIAS grevir_packet)
set_target_properties(grevir_packet PROPERTIES EXPORT_NAME packet)
target_compile_features(grevir_packet INTERFACE cxx_std_23)
target_link_libraries(grevir_packet INTERFACE grevir::base)
target_include_directories(grevir_packet INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>)

include(GNUInstallDirs)
install(TARGETS grevir_packet EXPORT GrevirPacketTargets)
install(DIRECTORY src/ DESTINATION include FILES_MATCHING
  PATTERN "*.hpp" PATTERN "*.h")
install(EXPORT GrevirPacketTargets NAMESPACE grevir::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/grevir-packet)
install(FILES cmake/grevir-packet-config.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/grevir-packet)
install(FILES LICENSE.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/grevir-packet)

option(GREVIR_BUILD_COMPILE_CHECKS "Compile native headers and template users" OFF)
option(GREVIR_BUILD_HOST_TESTS "Run deterministic native behavior tests" OFF)
if(GREVIR_BUILD_HOST_TESTS)
  enable_testing()
endif()
if(GREVIR_BUILD_COMPILE_CHECKS OR GREVIR_BUILD_HOST_TESTS)
  add_subdirectory(tests)
endif()
