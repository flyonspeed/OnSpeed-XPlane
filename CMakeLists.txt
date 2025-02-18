cmake_minimum_required(VERSION 3.10)
project(AOAAudio)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add SDK include directories
include_directories(${CMAKE_SOURCE_DIR}/SDK/CHeaders)

# Define the plugin target
add_library(AOAAudio SHARED
    aoa_audio.cpp
)

# Set output name and prefix based on platform
if(APPLE)
    set_target_properties(AOAAudio PROPERTIES PREFIX "")
    set_target_properties(AOAAudio PROPERTIES SUFFIX ".xpl")
elseif(WIN32)
    set_target_properties(AOAAudio PROPERTIES PREFIX "")
    set_target_properties(AOAAudio PROPERTIES SUFFIX ".xpl")
else()
    set_target_properties(AOAAudio PROPERTIES PREFIX "")
    set_target_properties(AOAAudio PROPERTIES SUFFIX ".xpl")
endif()

# Link against X-Plane SDK if needed
if(APPLE)
    target_link_libraries(AOAAudio "-framework XPLM")
endif()

find_package(OpenAL REQUIRED)
target_link_libraries(AOAAudio OpenAL::OpenAL) 