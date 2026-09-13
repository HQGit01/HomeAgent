# 原生 ESP-IDF 项目：idf.py -B build/lesson0 -DCOURSE_LESSON=0 build。
cmake_minimum_required(VERSION 3.16)
set(COURSE_LESSON "0" CACHE STRING "Lesson number: 0..17 or 99 (self-test)")
if(NOT COURSE_LESSON MATCHES "^([0-9]|1[0-7]|99)$")
    message(FATAL_ERROR "COURSE_LESSON must be 0..17 or 99")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(pocket_pet_course)
