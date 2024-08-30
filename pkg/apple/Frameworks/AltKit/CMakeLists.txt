cmake_minimum_required(VERSION 3.15.1)

project(AltKit LANGUAGES C Swift)

add_library(CAltKit
    Sources/CAltKit/NSError+ALTServerError.h
    Sources/CAltKit/NSError+ALTServerError.m
)

add_library(AltKit
    Sources/AltKit/Extensions/ALTServerError+Conveniences.swift
    Sources/AltKit/Extensions/Result+Conveniences.swift

    Sources/AltKit/Server/Connection.swift
    Sources/AltKit/Server/NetworkConnection.swift
    Sources/AltKit/Server/Server.swift
    Sources/AltKit/Server/ServerConnection.swift
    Sources/AltKit/Server/ServerManager.swift
    Sources/AltKit/Server/ServerProtocol.swift

    Sources/AltKit/Types/CodableServerError.swift
)

target_link_libraries(AltKit PRIVATE CAltKit)

set_property(TARGET AltKit PROPERTY XCODE_ATTRIBUTE_SWIFT_VERSION "5.0")

# Make CAltKit's modulemap available to AltKit
set_property(TARGET AltKit PROPERTY XCODE_ATTRIBUTE_SWIFT_INCLUDE_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/Sources/CAltKit")

# Add binary dir to interface include path to make Swift header accessible to targets using AltKit
target_include_directories(AltKit INTERFACE ${CMAKE_CURRENT_BINARY_DIR})

# Copy generated Swift header to binary dir
add_custom_command(TARGET AltKit
                   POST_BUILD
                   COMMAND cp $DERIVED_SOURCES_DIR/AltKit-Swift.h ${CMAKE_CURRENT_BINARY_DIR}
)
