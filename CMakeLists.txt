cmake_minimum_required(VERSION 3.10)
project(ajsonlibrary VERSION 0.1.1)

# Options
option(DEBUG "Enable debugging" OFF)
option(ADDRESS_SANITIZER "Enable Address Sanitizer" OFF)

set(CMAKE_INSTALL_INCLUDEDIR include)
set(CMAKE_INSTALL_BINDIR bin)
set(CMAKE_INSTALL_DOCDIR share/doc/ajsonlibrary)
set(CMAKE_C_STANDARD 99)

# Set C standard
set(CMAKE_C_STANDARD 99)

# Find the required libraries
find_package(amemorylibrary REQUIRED)
find_package(themacrolibrary REQUIRED)

# Compiler options
if(ADDRESS_SANITIZER)
    add_compile_options(-fsanitize=address)
endif()

# Source files
set(SOURCE_FILES src/ajson.c)

# Debug library
add_library(ajsonlibrary_debug STATIC ${SOURCE_FILES})
target_include_directories(ajsonlibrary_debug PUBLIC ${themacrolibrary_INCLUDE_DIRS})
target_include_directories(ajsonlibrary_debug PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_definitions(ajsonlibrary_debug PUBLIC -D_AML_DEBUG_)
target_compile_options(ajsonlibrary_debug PRIVATE -g -O0)
set_target_properties(ajsonlibrary_debug PROPERTIES OUTPUT_NAME "ajsonlibrary_debug")
target_compile_options(ajsonlibrary_debug PRIVATE -g -O0)


# Release library
add_library(ajsonlibrary_static STATIC ${SOURCE_FILES})
target_include_directories(ajsonlibrary_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_include_directories(ajsonlibrary_static PUBLIC ${themacrolibrary_INCLUDE_DIRS})
target_compile_options(ajsonlibrary_static PRIVATE -g -O3)
set_target_properties(ajsonlibrary_static PROPERTIES OUTPUT_NAME "ajsonlibrary_static")
target_compile_options(ajsonlibrary_static PRIVATE -O3)

# Shared library
add_library(ajsonlibrary SHARED ${SOURCE_FILES})
target_include_directories(ajsonlibrary PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_include_directories(ajsonlibrary PUBLIC ${themacrolibrary_INCLUDE_DIRS})
target_compile_options(ajsonlibrary PRIVATE -g -O3)
set_target_properties(ajsonlibrary PROPERTIES OUTPUT_NAME "ajsonlibrary")
target_compile_options(ajsonlibrary PRIVATE -O3)

# Link libraries
target_link_libraries(ajsonlibrary_debug PUBLIC amemorylibrary)
target_link_libraries(ajsonlibrary_static PUBLIC amemorylibrary)
target_link_libraries(ajsonlibrary PUBLIC amemorylibrary)

# Installation of the library
install(TARGETS ajsonlibrary ajsonlibrary_static ajsonlibrary_debug
        EXPORT ajsonlibraryTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        PUBLIC_HEADER DESTINATION include)

# Export the targets
install(EXPORT ajsonlibraryTargets
        FILE ajsonlibraryTargets.cmake
        DESTINATION lib/cmake/ajsonlibrary)

# Create and install the config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "ajsonlibraryConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

configure_file(ajsonlibraryConfig.cmake.in ajsonlibraryConfig.cmake @ONLY)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ajsonlibraryConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/ajsonlibraryConfigVersion.cmake"
        DESTINATION lib/cmake/ajsonlibrary)

# Installation of other files
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/a-json-library DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES README.md AUTHORS NEWS.md CHANGELOG.md LICENSE NOTICE DESTINATION ${CMAKE_INSTALL_DOCDIR})

# Uninstall command
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/bin/cmake_uninstall.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
               IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
