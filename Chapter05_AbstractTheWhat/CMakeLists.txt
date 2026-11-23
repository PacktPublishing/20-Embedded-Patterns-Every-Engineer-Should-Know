cmake_minimum_required(VERSION 3.16)
project(Chapter5ImagingCapabilities LANGUAGES CXX)

add_executable(chapter5_imaging_example
    examples/main.cpp
    include/ImagingCapabilities/AdjustIntegration.h
    include/ImagingCapabilities/Defaults.h
    include/ImagingCapabilities/Geometry.h
    include/ImagingCapabilities/IAreaOfInterest.h
    include/ImagingCapabilities/IBinning.h
    include/ImagingCapabilities/IGain.h
    include/ImagingCapabilities/IImageSource.h
    include/ImagingCapabilities/IIntegration.h
    include/ImagingCapabilities/IOffset.h
    include/ImagingCapabilities/ImageBlock.h
    include/ImagingCapabilities/Range.h
)

target_compile_features(chapter5_imaging_example PRIVATE cxx_std_20)
target_include_directories(chapter5_imaging_example PRIVATE include)
