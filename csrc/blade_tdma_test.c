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
#include <stdbool.h>
#include <tdma.h>
#include <liquid/liquid.h>

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




/* Callback typedef that just returns the bits of the frame */
/* TODO: write this a bit better */
//typedef void (*tdma_cb_rx_frame)(u8* frame_bits,u32 slot_i, slot_t * slot, tdma_t * tdma, void * cb_data);

/* Callback typedef when TDMA is ready to schedule a new frame */
/* Returns 1 if a frame is supplied, 0 if not */
/* If no frame supplied, slot is changed out of TX mode */
//typedef int (*tdma_cb_tx_frame)(u8* frame_bits,u32 slot_i, slot_t * slot, tdma_t * tdma, void * cb_data);

/* Callback to the radio front end to schedule a burst of TX samples */
//typedef int (*tdma_cb_tx_burst)(COMP* samples, size_t n_samples,i64 timestamp,void * cb_data);

int cb_tx_burst(COMP* samples, size_t n_samples,i64 timestamp,void * cb_data){

}

void cb_rx_frame(u8* frame_bits,u32 slot_i, slot_t * slot, tdma_t * tdma, void * cb_data){
    printf("Valid frame, slot %d, master %d\n",slot_i,slot->master_count);
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

    struct TDMA_MODE_SETTINGS mode = FREEDV_4800T;
    tdma_t * tdma = tdma_create(mode);
    tdma_set_rx_cb(tdma,cb_rx_frame,NULL);
    tdma_set_tx_burst_cb(tdma,cb_tx_burst,NULL);

    int nin = tdma_nin(tdma);
    float Fs_tdma = (float)mode.samp_rate;
    double Fs_bb = 960e3;
    double Fc = 445e6;
    double band_shift = 45e3;
    //float rs_ratio = Fs_bb / Fs_tdma;
    int rrs_ratio = 20;
    int nin_bb = nin*rrs_ratio;

    nco_crcf downmixer = nco_crcf_create(LIQUID_NCO);
    nco_crcf upmixer = nco_crcf_create(LIQUID_NCO);
    nco_crcf_set_phase(downmixer, 0.0f);
    nco_crcf_set_phase(upmixer, 0.0f);
    nco_crcf_set_frequency(downmixer,2.*M_PI*(band_shift/Fs_bb));
    nco_crcf_set_frequency(upmixer,band_shift/Fs_bb);
    int decim_len = 21;

    //float filter_taps[decim_len]
    //firdecim_crcf decim_filter = firdecim_crcf_create_prototype(rrs_ratio,20,50);
    msresamp_crcf decim_filter = msresamp_crcf_create(1.0/((float)rrs_ratio),50.0f);

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
    if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, Fs_bb) != 0)
    {
        printf("setSampleRate fail: %s\n", SoapySDRDevice_lastError());
    }
    if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, Fc+1e6, NULL) != 0)
    {
        printf("setFrequency fail: %s\n", SoapySDRDevice_lastError());
    }
    if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, Fs_bb) != 0)
    {
        printf("setSampleRate fail: %s\n", SoapySDRDevice_lastError());
    }
    if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, Fc, NULL) != 0)
    {
        printf("setFrequency fail: %s\n", SoapySDRDevice_lastError());
    }

    if(SoapySDRDevice_setGainElement(sdr,SOAPY_SDR_RX,0,"LNA",3) != 0)
        printf("setGainElement fail: %d\n",SoapySDRDevice_lastError());


    if(SoapySDRDevice_setGainElement(sdr,SOAPY_SDR_RX,0,"VGA1",18) != 0)
        printf("setGainElement fail: %d\n",SoapySDRDevice_lastError());

    if(SoapySDRDevice_setGainElement(sdr,SOAPY_SDR_RX,0,"VGA2",16) != 0)
        printf("setGainElement fail: %d\n",SoapySDRDevice_lastError());

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
    complex float slot_bbrx_buffer[nin_bb];
    complex float slot_bbrx_buffer_dm[nin_bb];
    complex float slot_rx_buffer[nin];
    int flags; //flags set by receive operation
    long long timeNs; //timestamp for receive buffer
    long long burstTimeNs = 0;
    long long timeNsStart;
    size_t bursts = 0;
    printf("Running\n");
    FILE * sampfile = fopen("samps.cf32","w+");
    for(size_t i = 0; bursts<30; i++){
        int nsamp = 0;
        int num_written_decim = 0;
        flags=0;
        bool first_loop = true;
        while(nsamp < nin_bb){
            void *buffs[] = { &slot_bbrx_buffer[nsamp] }; //array of rx buffers
            int ret = SoapySDRDevice_readStream(sdr, rxStream, buffs, nin_bb - nsamp, &flags, &timeNs, 100000);
            if(first_loop) timeNsStart = timeNs;
            first_loop = false;
            //printf("RX ret=%d, flags=%d, timeNs=%lld burstNs=%lld, nsamp=%d\n", ret, flags, timeNs, burstTimeNs,nsamp);
            if(ret < 0) {
                printf("err: %d\n",ret);
                break;
            }
            nsamp += ret;

        }
        /*  Mix and downsample */
        nco_crcf_mix_block_down(downmixer,slot_bbrx_buffer,slot_bbrx_buffer_dm,nin_bb);
        //msresamp_crcf_decim_execute(decim_filter,slot_bbrx_buffer_dm,nin_bb,slot_rx_buffer,&num_written_decim);
        msresamp_crcf_execute(decim_filter,slot_bbrx_buffer_dm,nin_bb,slot_rx_buffer,&num_written_decim);
        /* Convert nanosecond timestamp to 48k samples */
        i64 ts_48k = (timeNsStart*3)/62500;
        /* Give samps to TDMA */
        tdma_rx(tdma,slot_rx_buffer,ts_48k);

        //fwrite(slot_bbrx_buffer_dm,sizeof(complex float),nin_bb,sampfile);
        fwrite(slot_rx_buffer,sizeof(complex float),nin,sampfile);

        if(burstTimeNs == 0){
            burstTimeNs = timeNs;
        }
        if(timeNs > burstTimeNs){
            void *tb1[] = {sine};
            void *tb2[] = {zeros};
            flags=SOAPY_SDR_END_BURST;
            //int ret = SoapySDRDevice_writeStream(sdr,txStream,tb1,8192,&flags,burstTimeNs+10e6,100000);
            //printf(">>>>TX ret=%d, flags=%d, timeNs=%lld\n", ret, flags, burstTimeNs);
            //ret = SoapySDRDevice_writeStream(sdr,txStream,tb2,4096,&flags,burstTimeNs+10e6,100000);
            //printf(">>>>TX ret=%d, flags=%d, timeNs=%lld\n", ret, flags, burstTimeNs);
            burstTimeNs += 200e6;
            bursts++;
        }
    }
    fclose(sampfile);

    //shutdown the stream
    SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0); //stop streaming
    SoapySDRDevice_closeStream(sdr, rxStream);

    SoapySDRDevice_deactivateStream(sdr, txStream, 0, 0); //stop streaming
    SoapySDRDevice_closeStream(sdr, txStream);


    //cleanup device handle
    SoapySDRDevice_unmake(sdr);
    printf("Done\n");
    return EXIT_SUCCESS;

}