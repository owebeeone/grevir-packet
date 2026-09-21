file(GLOB_RECURSE public_headers CONFIGURE_DEPENDS RELATIVE
  "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/*.hpp"
  "${PROJECT_SOURCE_DIR}/src/*.h")
set(header_sources)
foreach(header IN LISTS public_headers)
  string(MAKE_C_IDENTIFIER "${header}" identifier)
  set(source "${CMAKE_CURRENT_BINARY_DIR}/${identifier}.cpp")
  file(WRITE "${source}" "#include <${header}>\n")
  list(APPEND header_sources "${source}")
endforeach()

add_library(grevir_packet_compile OBJECT native_compile.cpp ${header_sources})
target_link_libraries(grevir_packet_compile PRIVATE grevir::packet)
set_target_properties(grevir_packet_compile PROPERTIES CXX_EXTENSIONS OFF)
