include(CMakeFindDependencyMacro)
find_dependency(grevir-base CONFIG)
include("${CMAKE_CURRENT_LIST_DIR}/GrevirPacketTargets.cmake")
