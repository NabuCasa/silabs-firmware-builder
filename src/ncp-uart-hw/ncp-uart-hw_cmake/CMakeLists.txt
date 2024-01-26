# Define minimal required version of CMake.
cmake_minimum_required(VERSION "3.25")

# Project definition
project(
	ncp-uart-hw
	VERSION 1.0
	LANGUAGES C CXX ASM
)

# Include the definition of the slc_ncp-uart-hw target,
# which contains the content of the SLC project
include(ncp-uart-hw.cmake)

add_executable(ncp-uart-hw
    # Add additional sources here
)

target_include_directories(ncp-uart-hw PUBLIC
    # Add additional include paths here
)

target_compile_definitions(ncp-uart-hw PUBLIC
    # Add additional macros here
)

target_compile_options(ncp-uart-hw PUBLIC
    # Set additional compiler flags here
)

target_link_options(ncp-uart-hw PUBLIC
    # Set additional linker flags here
)

# Link with the content defined in the SLC project
target_link_libraries(ncp-uart-hw PRIVATE
    slc_ncp-uart-hw
)

# Include managed project content if available
include(ncp-uart-hw_project.cmake OPTIONAL RESULT_VARIABLE managed_project)
if(managed_project)
    message(STATUS "Using managed project content from ${managed_project}")
endif()

# Force the gcc linker command
set_target_properties(ncp-uart-hw PROPERTIES LINKER_LANGUAGE C)

# Create .bin, .hex and .s37 artifacts after building the project
add_custom_command(TARGET ncp-uart-hw
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O srec "$<TARGET_FILE:ncp-uart-hw>" "$<TARGET_FILE_DIR:ncp-uart-hw>/$<TARGET_FILE_BASE_NAME:ncp-uart-hw>.s37"
    COMMAND ${CMAKE_OBJCOPY} -O ihex "$<TARGET_FILE:ncp-uart-hw>" "$<TARGET_FILE_DIR:ncp-uart-hw>/$<TARGET_FILE_BASE_NAME:ncp-uart-hw>.hex"
    COMMAND ${CMAKE_OBJCOPY} -O binary "$<TARGET_FILE:ncp-uart-hw>" "$<TARGET_FILE_DIR:ncp-uart-hw>/$<TARGET_FILE_BASE_NAME:ncp-uart-hw>.bin" 
)

# Run post-build pipeline to perform additional post-processing
if(post_build_command)
add_custom_command(TARGET ncp-uart-hw
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
    COMMAND ${post_build_command}
)
endif()