cmake_minimum_required(VERSION 3.10)
project(CGAssignment2-improve)

find_package(OpenCV REQUIRED)

set(CMAKE_CXX_STANDARD 17)

include_directories(/usr/local/include)
include_directories("D:/eigen-3.4.0")


add_executable(CGAssignment2-improve main.cpp rasterizer.hpp rasterizer.cpp global.hpp Triangle.hpp Triangle.cpp)
target_link_libraries(CGAssignment2-improve ${OpenCV_LIBRARIES})

set(OpenCV_DIR "D:/Opencv/opencv/build")