cmake_minimum_required(VERSION 3.10)

# Project Configuration
project(ajsonlibrary VERSION 0.1.1)

# Variables
set(INCLUDE_DIR_NAME "a-json-library")
set(EXTRA_FILES README.md AUTHORS NEWS.md CHANGELOG.md LICENSE NOTICE)
set(CUSTOM_PACKAGES amemorylibrary themacrolibrary)

# Source files
file(GLOB SOURCE_FILES src/*.c)

find_package(a-cmake-library REQUIRED)

include(LibraryConfig)
include(LibraryBuild)
