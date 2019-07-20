
project(freedv_misc C)

cmake_minimum_required(VERSION 2.8)

include_directories( csrc/freedv-tdma/ )

# The chunk of the Codec2/FreeDV project extracted to make (cross)compiling easier
set(tdmaSources     csrc/tdma_testframer.c 
                    csrc/freedv-tdma/tdma.c
                    csrc/freedv-tdma/fsk.c 
                    csrc/freedv-tdma/modem_stats.c
                    csrc/freedv-tdma/golay23.c 
                    csrc/freedv-tdma/kiss_fft.c)

add_executable(bladerf_test csrc/blade_rf_test.c)
target_link_libraries(bladerf_test SoapySDR fftw3f m)

add_executable(tdma_bladerf csrc/blade_tdma_test.c ${tdmaSources})
target_include_directories(tdma_bladerf PUBLIC /usr/local/lib/)

find_library(CODEC2_LIB codec2)
target_link_libraries(tdma_bladerf SoapySDR liquid fftw3f m)

add_executable(tdma_soapy csrc/tdma_soapy.c csrc/tdma_testframer.h csrc/tdma_testframer.c ${tdmaSources})
target_include_directories(tdma_soapy PUBLIC /usr/local/lib/)

target_link_libraries(tdma_soapy SoapySDR liquid m fftw3f jansson)


