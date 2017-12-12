/*---------------------------------------------------------------------------*\

  FILE........: soapy_tdma.c
  AUTHOR......: Brady O'Brien
  DATE CREATED: 24 September 2017


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

#ifndef __SOAPY_TDMA_H
#define __SOAPY_TDMA_H

#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <tdma.h>
#include <stdbool.h>
#include <stdint.h>
#include <liquid/liquid.h>

typedef struct {
    SoapySDRStream * tx_stream;     /* Soapy stream for TX */
    SoapySDRStream * rx_stream;     /* Soapy stream for RX */
    SoapySDRDevice * sdr;           /* Soapy SDR device */
    bool rx_only_mode;              /* Flag set if this modem is only rx-ing TDMA frames */
    tdma_t * tdma;                  /* TDMA modem */
    msresamp_crcf  decim;          /* Radio -> TDMA decimator */
    msresamp_crcf  interp;         /* TDMA -> radio interpolator */
    nco_crcf downmixer;
    nco_crcf upmixer;



    double radio_fs;                /* SDR sample rate */
    double radio_fc;                /* SDR center frequency */
    double shift;                   /* Frequency shift */
    size_t mtu;                     /* MTU of tx stream */

    /* TX stuff */
    float complex * tx_buf_bb;
    uint64_t tx_time;
    bool tx_ready;
    
} soapy_tdma_radio_t;

soapy_tdma_radio_t * soapy_tdma_create(SoapySDRDevice * sdr,tdma_t * tdma, double f_offset,int * err, bool rx_only);
int soapy_tdma_loop();

#endif