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
#include <liquid/liquid.h>
#include "soapy_tdma.h"

soapy_tdma_radio_t * soapy_tdma_create(SoapySDRDevice * sdr,
                                        tdma_t * tdma, double f_offset,int * err,bool rx_only){
    
    soapy_tdma_radio_t * radio = malloc(sizeof(soapy_tdma_radio_t));
    
    if(SoapySDRDevice_setupStream(sdr, radio->rx_stream, ))
}
