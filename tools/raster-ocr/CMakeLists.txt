cmake_minimum_required(VERSION 3.21)
project(QuadernoRasterOcr LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui)
set(ONNXRUNTIME_ROOT "" CACHE PATH "Unpacked ONNX Runtime headers and native library")
find_path(ONNXRUNTIME_INCLUDE onnxruntime_cxx_api.h
    PATHS "${ONNXRUNTIME_ROOT}/include" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)
find_library(ONNXRUNTIME_LIBRARY onnxruntime
    PATHS "${ONNXRUNTIME_ROOT}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)

add_library(onnxruntime SHARED IMPORTED)
set_target_properties(onnxruntime PROPERTIES IMPORTED_LOCATION "${ONNXRUNTIME_LIBRARY}")
add_executable(quaderno-raster-ocr main.cpp)
target_compile_features(quaderno-raster-ocr PRIVATE cxx_std_20)
target_include_directories(quaderno-raster-ocr SYSTEM PRIVATE "${ONNXRUNTIME_INCLUDE}")
target_link_libraries(quaderno-raster-ocr PRIVATE Qt6::Core Qt6::Gui onnxruntime)
set_target_properties(quaderno-raster-ocr PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN")
