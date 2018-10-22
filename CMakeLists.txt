cmake_minimum_required(VERSION 3.5)
project(libmeow_hash_cpp VERSION 0.1.0 LANGUAGES CXX)

add_library(meow_hash_cpp INTERFACE meowhash_cpp/meow_hash.hpp)

target_include_directories(meow_hash_cpp
    PUBLIC 
        $<INSTALL_INTERFACE:meowhash_cpp>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/meowhash_cpp>
)

target_compile_features(meow_hash_cpp PRIVATE cxx_std_17)
