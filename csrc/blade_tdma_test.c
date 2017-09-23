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
//typedef int (*tdma_cb_tx_burst)(tdma_t * tdma,COMP* samples, size_t n_samples,i64 timestamp,void * cb_data);

typedef struct {
    float complex *     tx_buffer;
    bool                have_tx;
    int64_t             tx_time;
} tdma_stuff_holder;

int cb_tx_frame (u8* frame_bits,u32 slot_i, slot_t * slot, tdma_t * tdma, void * cb_data){
    for(int i=0; i<88; i++){
        frame_bits[i] = rand()&0x1;
    }
    return 1;
}

int cb_tx_burst(tdma_t * tdma,float complex* samples, size_t n_samples,i64 timestamp,void * cb_data){
    tdma_stuff_holder * tx_stuff = (tdma_stuff_holder*) cb_data;
    tx_stuff->have_tx = true;
    tx_stuff->tx_time = timestamp;
    for(int i = 0; i < tdma_nout(tdma); i++){
        samples[i] = samples[i]*.45;
    }
    memcpy(tx_stuff->tx_buffer,samples,sizeof(float complex)*tdma_nout(tdma));
}

void cb_rx_frame(u8* frame_bits,u32 slot_i, slot_t * slot, tdma_t * tdma, void * cb_data){
    printf("Valid frame, slot %d, master %d\n",slot_i,slot->master_count);
}

/* Chunks taken from https://github.com/pothosware/SoapySDR/wiki/C_API_Example */
int main(int argc,char ** argv){

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

    SoapySDRKwargs args2 = {};
    SoapySDRKwargs_set(&args2,"buffers","64");
    struct TDMA_MODE_SETTINGS mode = FREEDV_4800T;
    tdma_t * tdma = tdma_create(mode);
    tdma_set_rx_cb(tdma,cb_rx_frame,NULL);
    tdma_set_tx_cb(tdma,cb_tx_frame,NULL);
    tdma->tx_multislot_delay = 3;

    int nin = tdma_nin(tdma);
    int nout = tdma_nout(tdma);
    float Fs_tdma = (float)mode.samp_rate;
    double Fs_bb = 960e3;
    double Fc = 445e6;
    double band_shift = 45e3;
    //float rs_ratio = Fs_bb / Fs_tdma;
    int rrs_ratio = 20;
    int nin_bb = nin*rrs_ratio;
    int nout_bb = nout*rrs_ratio;

    nco_crcf downmixer = nco_crcf_create(LIQUID_NCO);
    nco_crcf upmixer = nco_crcf_create(LIQUID_NCO);
    nco_crcf_set_phase(downmixer, 0.0f);
    nco_crcf_set_phase(upmixer, 0.0f);
    nco_crcf_set_frequency(downmixer,2.*M_PI*(band_shift/Fs_bb));
    nco_crcf_set_frequency(upmixer,2.*M_PI*(band_shift/Fs_bb));
    int decim_len = 21;

    tdma_stuff_holder tx_stuff;
    tx_stuff.tx_buffer = malloc(sizeof(complex float)*nout);
    tx_stuff.have_tx = false;
    tx_stuff.tx_time = 0;

    tdma_set_tx_burst_cb(tdma,cb_tx_burst,(void*)&tx_stuff);

    //float filter_taps[decim_len]
    //firdecim_crcf decim_filter = firdecim_crcf_create_prototype(rrs_ratio,20,50);
    msresamp_crcf decim_filter = msresamp_crcf_create(1.0/((float)rrs_ratio),50.0f);
    msresamp_crcf interp_filter = msresamp_crcf_create((float)rrs_ratio,50.0f);

    //apply settings
    if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, Fs_bb) != 0)
    {
        printf("setSampleRate fail: %s\n", SoapySDRDevice_lastError());
    }
    if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, Fc, NULL) != 0)
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

    if (SoapySDRDevice_setupStream(sdr, &txStream, SOAPY_SDR_TX, SOAPY_SDR_CF32, NULL, 0, &args2) != 0)
    {
        printf("setupStream fail: %s\n", SoapySDRDevice_lastError());
    }
    //if(SoapySDRDevice_setupStream())

    SoapySDRDevice_activateStream(sdr, txStream, 0,0,0);    
    SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0); //start streaming
    //create a re-usable buffer for rx samples
    complex float slot_bbrx_buffer[nin_bb];
    complex float slot_bbrx_buffer_dm[nin_bb];
    complex float slot_bbtx_buffer[nout_bb];
    complex float slot_bbtx_buffer_dm[nout_bb];
    complex float slot_rx_buffer[nin];

    int flags_rx;
    int flags_tx;
    i64 timeNsRx;
    i64 timeStartNsRx;
    i64 ts_tx_ns;
    i64 ts_tx_48k;
    i64 ts_rx_48k;
    int mtu_tx = SoapySDRDevice_getStreamMTU(sdr, txStream);
    int nsamp_rx = 0;
    int nsamp_tx = 0;
    int n_written_decim;
    bool first_rx_loop;
    int ret_rx;
    int ret_tx;
    bool tx_done,rx_done;

    bool tx_started = false;
    printf("Running\n");
    for(size_t i = 0; i<30000; i++){
        if(i>200 && !tx_started){
            if(tdma_get_slot(tdma,0)->state == rx_sync){
                tdma_start_tx(tdma,1);
                tx_started = true;
                printf("Starting TX, slot 1\n");
            }else if(tdma_get_slot(tdma,1)->state == rx_sync){
                tdma_start_tx(tdma,0);
                tx_started = true;
                printf("Starting TX, slot 0\n");
            }
            //tdma_start_tx(tdma,0);
        }

        /* If we have TX frame, upconvert for radio */
        if(tx_stuff.have_tx){
            msresamp_crcf_execute(interp_filter, tx_stuff.tx_buffer, nout, slot_bbtx_buffer_dm, &n_written_decim);
            nco_crcf_mix_block_up(upmixer,slot_bbtx_buffer_dm,slot_bbtx_buffer,nout_bb);
            ts_tx_ns = (tx_stuff.tx_time*62500)/3;
        }

        tx_done = rx_done = false;
        /* Do RX and TX with radio */
        tx_done = !tx_stuff.have_tx;
        first_rx_loop = true;
        nsamp_rx = nsamp_tx = 0;
        while(!(tx_done&&rx_done)){
            if(!rx_done){
                void *rx_buffs[] = { &slot_bbrx_buffer[nsamp_rx] }; 
                flags_rx = 0;
                int ret_rx =  SoapySDRDevice_readStream(sdr, rxStream, rx_buffs, nin_bb - nsamp_rx, &flags_rx, &timeNsRx, 100000);
                if(ret_rx < 0){
                    printf("err: %d\n",ret_rx);
                    rx_done = true;
                }
                nsamp_rx += ret_rx;
                if(nsamp_rx >= nin_bb) rx_done = true;
                if(first_rx_loop)
                    timeStartNsRx = timeNsRx;
                first_rx_loop = false;
            }
            if(!tx_done){
                void *tx_buffs[] = { &slot_bbtx_buffer[nsamp_tx] };
                flags_tx = SOAPY_SDR_END_BURST;
                if(nsamp_tx == 0) flags_tx |= SOAPY_SDR_HAS_TIME;
                ret_tx = SoapySDRDevice_writeStream(sdr, txStream, tx_buffs, (nout_bb - nsamp_tx), &flags_tx, ts_tx_ns,100000);
                if(ret_tx < 0){
                    printf("err: %d\n",ret_tx);
                    tx_done = true;
                }
                nsamp_tx += ret_tx;
                if(nsamp_tx >= nout_bb) tx_done = true;
            }
        }
        tx_stuff.have_tx = false;

        /* Downconvert RX frame and give to TDMA stack */

        nco_crcf_mix_block_down(downmixer, slot_bbrx_buffer, slot_bbrx_buffer_dm, nin_bb);
        msresamp_crcf_execute(decim_filter, slot_bbrx_buffer_dm, nin_bb, slot_rx_buffer, &n_written_decim);
        ts_rx_48k = (timeStartNsRx*3)/62500;
        tdma_rx(tdma,slot_rx_buffer,ts_rx_48k);
    }

    /*
    int flags; //flags set by receive operation
    long long timeNs; //timestamp for receive buffer
    long long burstTimeNs = 0;
    long long timeNsStart;
    size_t bursts = 0;
    printf("Running\n");
    bool tx_started = false;
    int mtu_tx = SoapySDRDevice_getStreamMTU(sdr, txStream);
    //FILE * sampfile = fopen("samps.cf32","w+");
    for(size_t i = 0; bursts<3000; i++){
        int nsamp = 0;
        int num_written_decim = 0;
        flags=0;
        bool first_loop = true;
        while(nsamp < nin_bb){
            void *rx_buffs[] = { &slot_bbrx_buffer[nsamp] }; //array of rx buffers
            int ret = SoapySDRDevice_readStream(sdr, rxStream, rx_buffs, nin_bb - nsamp, &flags, &timeNs, 100000);
            if(first_loop) timeNsStart = timeNs;
            first_loop = false;
            //printf("RX ret=%d, flags=%d, timeNs=%lld burstNs=%lld, nsamp=%d\n", ret, flags, timeNs, burstTimeNs,nsamp);
            if(ret < 0) {
                printf("err: %d\n",ret);
                break;
            }
            nsamp += ret;

        }
        bursts++;

        nco_crcf_mix_block_down(downmixer,slot_bbrx_buffer,slot_bbrx_buffer_dm,nin_bb);
        //msresamp_crcf_decim_execute(decim_filter,slot_bbrx_buffer_dm,nin_bb,slot_rx_buffer,&num_written_decim);
        msresamp_crcf_execute(decim_filter,slot_bbrx_buffer_dm,nin_bb,slot_rx_buffer,&num_written_decim);

        i64 ts_48k = (timeNsStart*3)/62500;

        tdma_rx(tdma,slot_rx_buffer,ts_48k);

        //fwrite(slot_bbrx_buffer_dm,sizeof(complex float),nin_bb,sampfile);
        //fwrite(slot_rx_buffer,sizeof(complex float),nin,sampfile);

        if(bursts>120 && !tx_started){
            printf("Starting TX\n");
            tdma_start_tx(tdma,1);
            //tdma_start_tx(tdma,0);
            tx_started = true;
        }
        
        if(tx_stuff.have_tx){
            msresamp_crcf_execute(interp_filter,tx_stuff.tx_buffer,nout,slot_bbrx_buffer_dm,&num_written_decim);
            nco_crcf_mix_block_up(upmixer,slot_bbrx_buffer_dm,slot_bbrx_buffer,nout_bb);
            i64 ts_ns = (tx_stuff.tx_time*62500)/3;
            i64 dts_ns = 0;
            int ret = 0;
            nsamp = 0;
            while(nsamp < nout_bb){
                void *tx_buffs[] = { &slot_bbrx_buffer[nsamp] };
                void *rx_buffs[] = { &slot_bbrx_buffer_dm[0] };
                flags = SOAPY_SDR_END_BURST;
                if(nsamp==0) flags |= SOAPY_SDR_HAS_TIME;
                ret = SoapySDRDevice_writeStream(sdr, txStream, tx_buffs, (nout_bb - nsamp), &flags, ts_ns+10e6,100000);
                      //SoapySDRDevice_readStream(sdr, rxStream, rx_buffs, nin_bb - nsamp, &flags, &timeNs, 100000);
                
                if( ret < 0 ) {
                    printf("err: %d\n",ret);
                    break;
                }
                dts_ns = (nsamp*62500)/3;
                nsamp += ret;
            }
            void *tx_buffs[] = { &zeros[0] };
            flags = SOAPY_SDR_END_BURST;
            //SoapySDRDevice_writeStream(sdr, txStream, tx_buffs, 4096, &flags, ts_ns+dts_ns+100e6,100000);
            tx_stuff.have_tx = false;
        }
        
    }
   // fclose(sampfile);

   */
    //shutdown the stream
    SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0); //stop streaming
    SoapySDRDevice_closeStream(sdr, rxStream);

    SoapySDRDevice_deactivateStream(sdr, txStream, 0, 0); //stop streaming
    SoapySDRDevice_closeStream(sdr, txStream);

    //cleanup device handle
    SoapySDRDevice_unmake(sdr);

    tdma_destroy(tdma);
    nco_crcf_destroy(downmixer);
    nco_crcf_destroy(upmixer);
    msresamp_crcf_destroy(decim_filter);

    printf("Done\n");
    return EXIT_SUCCESS;

}