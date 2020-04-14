cmake_minimum_required(VERSION 3.4.1)

include_directories(src/main/cpp/include)

set(FF ${CMAKE_CURRENT_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI})

add_library(libyuv STATIC IMPORTED)
set_target_properties(libyuv PROPERTIES IMPORTED_LOCATION ${FF}/libyuv.a)

#add_library(apm-plugin-raw-data-api-java
#        SHARED
#        src/main/cpp/media_preprocessing_plugin_jni.cpp)

add_library(apm-plugin-video-raw-data
        SHARED
        src/main/cpp/apm-plugin-video-raw-data.cpp
        )

find_library(log-lib log)

target_link_libraries(apm-plugin-video-raw-data libyuv ${log-lib})