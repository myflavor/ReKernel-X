cmake_minimum_required(VERSION 3.22.1)
project("ReKernel")

add_library(ReKernel SHARED rekernel_jni.cpp)

# 16 KB page size alignment for Android 15+ compatibility.
target_link_options(ReKernel PRIVATE "-Wl,-z,max-page-size=16384")

find_library(log-lib log)
target_link_libraries(ReKernel ${log-lib})
