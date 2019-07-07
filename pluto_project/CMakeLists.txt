
project(freedv_misc C)

cmake_minimum_required(VERSION 2.8)

#TODO: Come up with better way to connect this to PlutoSDR buildroot
#TODO: Push modified pluto build files to Github
include(~/gitwks/plutosdr-fw/buildroot/output/host/share/buildroot/toolchainfile.cmake)

add_executable(tdma_pluto csrc/pluto_test.c 
                    ../csrc/tdma_testframer.c 
                    ../csrc/freedv-tdma/tdma.c
                    ../csrc/freedv-tdma/fsk.c 
                    ../csrc/freedv-tdma/modem_stats.c
                    ../csrc/freedv-tdma/golay23.c 
                    ../csrc/freedv-tdma/kiss_fft.c)

set(CMAKE_C_FLAGS "-O3 -ffast-math -mfpu=neon-vfpv3 -Wall -ftree-vectorizer-verbose=2 -ftree-vectorize")

target_include_directories(tdma_pluto PUBLIC ../csrc ../csrc/freedv-tdma)
target_link_libraries(tdma_pluto iio liquid m pthread)
