
project(freedv_misc C)

cmake_minimum_required(VERSION 2.8)


add_executable(bladerf_test csrc/blade_rf_test.c)
target_link_libraries(bladerf_test SoapySDR m)