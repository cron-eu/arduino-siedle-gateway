cmake_minimum_required(VERSION 3.8)

project(main)
arduino_cmake_project(main BOARD_NAME uno)

add_arduino_executable(main)
target_sketches(main main.ino)

find_arduino_library(scheduler_lib Scheduler ${board_id} 3RD_PARTY)
link_arduino_library(main scheduler_lib)

#find_arduino_library(wifinina_lib WiFiNINA 3RD_PARTY)
#link_arduino_library(main wifinina_lib)
