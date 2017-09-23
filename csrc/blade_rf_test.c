/*---------------------------------------------------------------------------*\

  FILE........: blade_rf_test.c
  AUTHOR......: Brady O'Brien
  DATE CREATED: 20 September 2017

  A little program to test my understanding of the SoapySDR API with full
  duplex SDRs

\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2017 Brady O'Brien

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2.1, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <stdio.h> //printf
#include <stdlib.h> //free
#include <complex.h>
#include <math.h>

#define N_SINE 100000
static float complex sine[N_SINE];
static float complex zeros[N_SINE];

void setupSineTable(){
    float Fs = 1e6;
    float F1 = 200e3;
    float w = 2*M_PI*(F1/Fs);
    for(size_t i = 0; i<N_SINE; i++){
        sine[i] = ccosf(i*w);
        sine[i] += csinf(i*w)*_Complex_I;
        zeros[i] = 0;
    }
}

/* Chunks taken from https://github.com/pothosware/SoapySDR/wiki/C_API_Example */
int main(int argc,char ** argv){
    size_t length;
    SoapySDRKwargs *results = SoapySDRDevice_enumerate(NULL,&length);
    for (size_t i = 0; i < length; i++)
    {
        printf("Found device #%d: ", (int)i);
        for (size_t j = 0; j < results[i].size; j++)
        {
            printf("%s=%s, ", results[i].keys[j], results[i].vals[j]);
        }
        printf("\n");
    }
    SoapySDRKwargsList_clear(results, length);

    setupSineTable();
    //create device instance
    //args can be user defined or from the enumeration result
    SoapySDRKwargs args = {};
    SoapySDRKwargs_set(&args, "driver", "bladerf");
    SoapySDRDevice *sdr = SoapySDRDevice_make(&args);
    SoapySDRKwargs_clear(&args);
    if (sdr == NULL)
    {
        printf("SoapySDRDevice_make fail: %s\n", SoapySDRDevice_lastError());
        return EXIT_FAILURE;
    }

    //query device info
    char** names = SoapySDRDevice_listAntennas(sdr, SOAPY_SDR_RX, 0, &length);
    printf("Rx antennas: ");
    for (size_t i = 0; i < length; i++) printf("%s, ", names[i]);
    printf("\n");
    SoapySDRStrings_clear(&names, length);

    names = SoapySDRDevice_listGains(sdr, SOAPY_SDR_RX, 0, &length);
    printf("Rx gains: ");
    for (size_t i = 0; i < length; i++) printf("%s, ", names[i]);
    printf("\n");
    SoapySDRStrings_clear(&names, length);

    SoapySDRRange *ranges = SoapySDRDevice_getFrequencyRange(sdr, SOAPY_SDR_RX, 0, &length);
    printf("Rx freq ranges: ");
    for (size_t i = 0; i < length; i++) printf("[%g Hz -> %g Hz], ", ranges[i].minimum, ranges[i].maximum);
    printf("\n");
    free(ranges);

    size_t channels = SoapySDRDevice_getNumChannels(sdr,SOAPY_SDR_RX);
    printf("RX channels: %d\n",channels);
    channels = SoapySDRDevice_getNumChannels(sdr,SOAPY_SDR_TX);
    printf("TX channels: %d\n",channels);

    SoapySDRKwargs channelInfo = SoapySDRDevice_getChannelInfo(sdr,SOAPY_SDR_RX,0);
    printf("RX Stream Info: ");
    for(size_t i = 0; i < channelInfo.size; i++){
        printf("%s=%s, ", channelInfo.keys[i], channelInfo.vals[i]);
    }
    printf("\n");

    channelInfo = SoapySDRDevice_getChannelInfo(sdr,SOAPY_SDR_TX,0);
    printf("TX Stream Info: ");
    for(size_t i = 0; i < channelInfo.size; i++){
        printf("%s=%s, ", channelInfo.keys[i], channelInfo.vals[i]);
    }
    printf("\n");

    bool fdx = SoapySDRDevice_getFullDuplex(sdr,SOAPY_SDR_TX,0);
    if(fdx) printf("Is FDX in TX direction\n");
    else    printf("Is not FDX in TX\n");
    fdx = SoapySDRDevice_getFullDuplex(sdr,SOAPY_SDR_RX,0);
    if(fdx) printf("Is FDX in RX direction\n");
    else    printf("Is not FDX in RX\n");

    //apply settings
    if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, 1e6) != 0)
    {
        printf("setSampleRate fail: %s\n", SoapySDRDevice_lastError());
    }
    if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, 446e6, NULL) != 0)
    {
        printf("setFrequency fail: %s\n", SoapySDRDevice_lastError());
    }
    if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, 1e6) != 0)
    {
        printf("setSampleRate fail: %s\n", SoapySDRDevice_lastError());
    }
    if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, 446e6, NULL) != 0)
    {
        printf("setFrequency fail: %s\n", SoapySDRDevice_lastError());
    }

    //setup a stream (complex floats)
    SoapySDRStream *rxStream;
    SoapySDRStream *txStream;
    if (SoapySDRDevice_setupStream(sdr, &rxStream, SOAPY_SDR_RX, SOAPY_SDR_CF32, NULL, 0, NULL) != 0)
    {
        printf("setupStream fail: %s\n", SoapySDRDevice_lastError());
    }

    if (SoapySDRDevice_setupStream(sdr, &txStream, SOAPY_SDR_TX, SOAPY_SDR_CF32, NULL, 0, NULL) != 0)
    {
        printf("setupStream fail: %s\n", SoapySDRDevice_lastError());
    }
    //if(SoapySDRDevice_setupStream())

    SoapySDRDevice_activateStream(sdr, txStream, 0,0,0);    
    SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0); //start streaming
    //create a re-usable buffer for rx samples
    complex float buff[8192];

    int flags; //flags set by receive operation
    long long timeNs; //timestamp for receive buffer


    long long burstTimeNs = 0;
    size_t bursts = 0;
    for(size_t i = 0; bursts<50; i++){
        void *buffs[] = {buff}; //array of rx buffers
        int ret;
        ret = SoapySDRDevice_readStream(sdr, rxStream, buffs, 4096, &flags, &timeNs, 100000);
        //printf("RX ret=%d, flags=%d, timeNs=%lld burstNs=%lld\n", ret, flags, timeNs, burstTimeNs);
        if(burstTimeNs == 0){
            burstTimeNs = timeNs;
        }
        if(timeNs > burstTimeNs){
            void *tb1[] = {sine};
            void *tb2[] = {zeros};
            flags=SOAPY_SDR_END_BURST;
            ret = SoapySDRDevice_writeStream(sdr,txStream,tb1,8192,&flags,burstTimeNs+10e6,100000);
            printf(">>>>TX ret=%d, flags=%d, timeNs=%lld\n", ret, flags, burstTimeNs);
            //ret = SoapySDRDevice_writeStream(sdr,txStream,tb2,4096,&flags,burstTimeNs+10e6,100000);
            //printf(">>>>TX ret=%d, flags=%d, timeNs=%lld\n", ret, flags, burstTimeNs);
            burstTimeNs += 200e6;
            bursts++;
        }
    }

    //cleanup device handle
    SoapySDRDevice_unmake(sdr);

    //shutdown the stream
    SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0); //stop streaming
    SoapySDRDevice_closeStream(sdr, rxStream);

    SoapySDRDevice_deactivateStream(sdr, txStream, 0, 0); //stop streaming
    SoapySDRDevice_closeStream(sdr, txStream);
    printf("Done\n");
    return EXIT_SUCCESS;

}