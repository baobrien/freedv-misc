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
#include <stdio.h> 
#include <stdlib.h> 
#include <complex.h>
#include <math.h>
#include <stdbool.h>
#include <tdma.h>
#include <jansson.h>
#include <liquid/liquid.h>

#include "tdma_testframer.h"

typedef struct {
    float complex *     tx_buffer;
    bool                have_tx;
    int64_t             tx_time;
} tdma_stuff_holder;

int cb_tx_burst(tdma_t * tdma,float complex* samples, size_t n_samples,i64 timestamp,void * cb_data){
    tdma_stuff_holder * tx_stuff = (tdma_stuff_holder*) cb_data;
    tx_stuff->have_tx = true;
    tx_stuff->tx_time = timestamp;
    for(int i = 0; i < tdma_nout(tdma); i++){
        samples[i] = samples[i]*.2;
    }
    memcpy(tx_stuff->tx_buffer,samples,sizeof(float complex)*tdma_nout(tdma));
}

static void json_error(json_error_t * err){
    fprintf(stderr,"Json Error line %d: %s \n",err->line,err->text);
}

/* like Jansson's json_is_true, but with null check */
inline static bool json_is_true_nc(json_t * v){
    if(v == NULL) return false;
    return json_is_true(v);
}

int main(int argc,char ** argv){

    char* config_filename;
    if(argc<2){
        config_filename = "radio_conf.json";
    }else{
        config_filename = argv[1];
    }

    json_error_t json_err;
    json_t * config_json = json_load_file(config_filename,0,&json_err);

    if(config_json == NULL){
        fprintf(stderr,"Couldn't open or decode %s\n",config_filename);
        json_error(&json_err);
        return EXIT_FAILURE;
    }

    json_t * rc_json = json_object_get(config_json,"radio");
    if(rc_json == NULL){
        fprintf(stderr,"Couldn't find radio parameter in config\n");
        return EXIT_FAILURE;
    }


    SoapySDRKwargs args = {};

    char * json_key;
    json_t * json_val;
    json_object_foreach(rc_json,json_key,json_val){
        char * val = json_string_value(json_val);
        if(val != NULL){
            SoapySDRKwargs_set(&args,json_key,val);
        }
    }

    SoapySDRDevice *sdr = SoapySDRDevice_make(&args);
    SoapySDRKwargs_clear(&args);
    if (sdr == NULL)
    {
        printf("SoapySDRDevice_make fail: %s\n", SoapySDRDevice_lastError());
        return EXIT_FAILURE;
    }

    double Fs_bb =      json_number_value(json_object_get(config_json,"samp_rate"));
    double Fc =         json_number_value(json_object_get(config_json,"rf_freq"));
    double band_shift = json_number_value(json_object_get(config_json,"bb_shift"));

    Fs_bb -= band_shift;

    bool enable_tx = json_is_true_nc(json_object_get(config_json,"sdr_tx_enable"));
    if(enable_tx)
        printf("TX Enabled!\n");
    else
        printf("TX Disabled!\n");

    struct TDMA_MODE_SETTINGS mode = FREEDV_4800T;
    tdma_t * tdma = tdma_create(mode);
    //tdma_set_rx_cb(tdma,cb_rx_frame,NULL);
    //tdma_set_tx_cb(tdma,cb_tx_frame,NULL);
    tdma->tx_multislot_delay = 3;

    tdma_test_framer * ttf = ttf_create(tdma);
    ttf->print_enable = true;

    int nin = tdma_nin(tdma);
    int nout = tdma_nout(tdma);
    float Fs_tdma = (float)mode.samp_rate;
    float rs_ratio = Fs_bb / Fs_tdma;
    float rrs_ratio = rs_ratio;
    //int rrs_ratio = 20;
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

    if(enable_tx)
        tdma_set_tx_burst_cb(tdma,cb_tx_burst,(void*)&tx_stuff);

    //float filter_taps[decim_len]
    //firdecim_crcf decim_filter = firdecim_crcf_create_prototype(rrs_ratio,20,50);
    msresamp_crcf decim_filter = msresamp_crcf_create(1.0/((float)rs_ratio),50.0f);
    msresamp_crcf interp_filter = msresamp_crcf_create((float)rs_ratio,50.0f);

    SoapySDRDevice_setBandwidth(sdr, SOAPY_SDR_RX,0, 5e6);
    SoapySDRDevice_setBandwidth(sdr, SOAPY_SDR_TX,0, 5e6);


    //apply settings
    if(enable_tx){
        if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_TX, 0, Fs_bb) != 0)
        {
            printf("setSampleRate fail: %s\n", SoapySDRDevice_lastError());
        }
        if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_TX, 0, Fc, NULL) != 0)
        {
            printf("setFrequency fail: %s\n", SoapySDRDevice_lastError());
        }
    }

    if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, Fs_bb) != 0)
    {
        printf("setSampleRate fail: %s\n", SoapySDRDevice_lastError());
    }
    if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, Fc, NULL) != 0)
    {
        printf("setFrequency fail: %s\n", SoapySDRDevice_lastError());
    }

    /* Set up gains */
    rc_json = json_object_get(config_json,"rx_gains");
    if(rc_json != NULL){
        json_object_foreach(rc_json,json_key,json_val){
            double val = json_number_value(json_val);
            if(SoapySDRDevice_setGainElement(sdr,SOAPY_SDR_RX,0,json_key,val) != 0){
                fprintf(stderr,"setGainElement RX %s=%f fail: %d\n",json_key,val,SoapySDRDevice_lastError());
            }
            
        }
    }
    rc_json = json_object_get(config_json,"tx_gains");
    if((rc_json != NULL) && enable_tx){
        json_object_foreach(rc_json,json_key,json_val){
            double val = json_number_value(json_val);
            if(SoapySDRDevice_setGainElement(sdr,SOAPY_SDR_TX,0,json_key,val) != 0){
                fprintf(stderr,"setGainElement TX %s=%f fail: %d\n",json_key,val,SoapySDRDevice_lastError());
            }
        }
    }

    /* Set up antennas */
    json_key = json_string_value(json_object_get(config_json,"rx_antenna"));
    if(json_key != NULL){
        if(SoapySDRDevice_setAntenna(sdr,SOAPY_SDR_RX,0,json_key)){
            fprintf(stderr,"failed to set RX antenna %s\n",json_key);
        }
    }

    json_key = json_string_value(json_object_get(config_json,"tx_antenna"));
    if(json_key != NULL && enable_tx){
        if(SoapySDRDevice_setAntenna(sdr,SOAPY_SDR_TX,0,json_key)){
           fprintf(stderr,"failed to set RX antenna %s\n",json_key);
        }
    }

    /* Setup testor settings */
    rc_json = json_object_get(config_json,"testor_settings");
    if(rc_json != NULL){
        ttf->tx_enable = json_is_true_nc(json_object_get(rc_json,"test_tx_enable"));
        ttf->tx_master = json_is_true_nc(json_object_get(rc_json,"test_tx_master"));
        ttf->tx_repeat = json_is_true_nc(json_object_get(rc_json,"test_tx_repeater"));
        ttf->print_enable = json_is_true_nc(json_object_get(rc_json,"test_rx_print"));
        ttf->tx_id = json_integer_value(json_object_get(rc_json,"test_station_id"));
    }else{
        fprintf(stderr,"RC_JSON NULL!\n");
    }

    //setup a stream (complex floats)
    SoapySDRStream *rxStream;
    SoapySDRStream *txStream;
    if (SoapySDRDevice_setupStream(sdr, &rxStream, SOAPY_SDR_RX, SOAPY_SDR_CF32, NULL, 0, NULL) != 0){
        fprintf(stderr,"setupStream fail: %s\n", SoapySDRDevice_lastError());
    }

    if(enable_tx){
        if (SoapySDRDevice_setupStream(sdr, &txStream, SOAPY_SDR_TX, SOAPY_SDR_CF32, NULL, 0, NULL) != 0){
            fprintf(stderr,"setupStream fail: %s\n", SoapySDRDevice_lastError());
        }   
    }


    if(enable_tx)    
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
    int mtu_tx = 0;
    if(enable_tx)
        mtu_tx = SoapySDRDevice_getStreamMTU(sdr, txStream);
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
            //tdma_start_tx(tdma,1);
            //tx_started = true;
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

        rx_done = false;
        /* Do RX and TX with radio */
        tx_done = ! (tx_stuff.have_tx && enable_tx);
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

    SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0); 
    SoapySDRDevice_closeStream(sdr, rxStream);

    if(enable_tx){
        SoapySDRDevice_deactivateStream(sdr, txStream, 0, 0); 
        SoapySDRDevice_closeStream(sdr, txStream);
    }

    SoapySDRDevice_unmake(sdr);

    ttf_destroy(ttf);
    tdma_destroy(tdma);
    nco_crcf_destroy(downmixer);
    nco_crcf_destroy(upmixer);
    msresamp_crcf_destroy(decim_filter);

    return EXIT_SUCCESS;

}