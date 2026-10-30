# file-format: 1.0.0
cmake_minimum_required(VERSION 3.30)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    Inc/${MCUFAMILY}
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  # Add additional source files here
  Src/usbdev_main.c
)
