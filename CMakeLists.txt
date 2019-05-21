
project(freedv_misc C)

cmake_minimum_required(VERSION 2.8)

include(~/gitwks/plutosdr-fw/buildroot/output/host/share/buildroot/toolchainfile.cmake)

# include_directories( freetel-code/codec2-dev/src )

# add_executable(bladerf_test csrc/blade_rf_test.c)
# target_link_libraries(bladerf_test SoapySDR m)


# link_directories( ../freetel-code/codec2-dev/build_linux/src )
# #add_executable(tdma_bladerf csrc/blade_tdma_test.c freetel-code/codec2-dev/src/tdma.h)
# #target_include_directories(tdma_bladerf PUBLIC freetel-code/codec2-dev/build_linux/src freetel-code/codec2-dev/src/ /usr/local/lib/)

# find_library(CODEC2_LIB codec2)
# #target_link_libraries(tdma_bladerf SoapySDR liquid m codec2)

# add_executable(tdma_lime csrc/lime_tdma_test.c csrc/tdma_testframer.h csrc/tdma_testframer.c freetel-code/codec2-dev/src/tdma.h)
# target_include_directories(tdma_lime PUBLIC freetel-code/codec2-dev/build_linux/srcfs freetel-code/codec2-dev/src/ /usr/local/lib/)

# target_link_libraries(tdma_lime SoapySDR liquid m codec2 jansson)i


add_executable(tdma_pluto csrc/pluto_test.c csrc/tdma_testframer.c 
                    csrc/freedv-tdma/tdma.c csrc/freedv-tdma/fsk.c 
                    csrc/freedv-tdma/modem_stats.c csrc/freedv-tdma/golay23.c 
                    csrc/freedv-tdma/kiss_fft.c)

set(CMAKE_C_FLAGS "-O3 -ffast-math -mfpu=neon-vfpv3 -Wall -ftree-vectorizer-verbose=2 -ftree-vectorize")
#target_include_directories(tdma_pluto PUBLIC freetel-code/codec2-dev/build_linux/src freetel-code/codec2-dev/src/ /usr/local/lib/)
target_include_directories(tdma_pluto PUBLIC csrc/freedv-tdma)
target_link_libraries(tdma_pluto iio liquid m)
