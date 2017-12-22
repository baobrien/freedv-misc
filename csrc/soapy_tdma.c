/*---------------------------------------------------------------------------*\

  FILE........: soapy_tdma.c
  AUTHOR......: Brady O'Brien
  DATE CREATED: 24 September 2017

  A small core to operate an instance of TDMA on a SoapySDR supporting radio

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
#include <tdma.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <liquid/liquid.h>
#include "soapy_tdma.h"

soapy_tdma_radio_t * soapy_tdma_create(SoapySDRDevice * sdr,
                                        tdma_t * tdma, float shift,int * err,bool rx_only){
    
    /* TODO malloc null checking and cleanup */
    soapy_tdma_radio_t * radio = malloc(sizeof(soapy_tdma_radio_t));

    
    radio->sdr = sdr;
    radio->shift = shift;

    if(SoapySDRDevice_setupStream(sdr, radio->rx_stream, SOAPY_SDR_RX, SOAPY_SDR_CF32, NULL, 0, NULL)){
      err = -1;
      goto soapy_tdma_create_err;
    }

    size_t mtu = SoapySDRDevice_getStreamMTU(sdr, radio->rx_stream);
    radio->mtu = mtu;
    /* Get sample rates; make sure radio rate is divisible by tdma rate */
    float Fs_sdr = SoapySDRDevice_getSampleRate(sdr, SOAPY_SDR_RX, 0);

    float Fs_bb = tdma->settings.samp_rate;
    float Fs_dr = fmodf(Fs_sdr,Fs_bb);  
    if( Fs_dr > .01 ){
      err = -1;
      goto soapy_tdma_create_err;
    }

    float R = Fs_sdr/Fs_bb;
    float filtDb = 60;

    
    nco_crcf downmixer = nco_crcf_create(LIQUID_NCO);
    nco_crcf upmixer = nco_crcf_create(LIQUID_NCO);
    nco_crcf_set_phase(downmixer, 0.0f);
    nco_crcf_set_phase(upmixer, 0.0f);
    nco_crcf_set_frequency(downmixer,2.*M_PI*(shift/Fs_sdr));
    nco_crcf_set_frequency(upmixer,2.*M_PI*(shift/Fs_sdr));

    msresamp_crcf decim  = msresamp_crcf_create(1.0/((float)R),filtDb);
    msresamp_crcf interp = msresamp_crcf_create(((float)R),filtDb);

    radio->decim = decim;
    radio->interp = interp;
    radio->downmixer = downmixer;
    radio->upmixer = upmixer;
    radio->rx_only_mode = rx_only;

    soapy_tdma_create_err:
    return NULL;
}
