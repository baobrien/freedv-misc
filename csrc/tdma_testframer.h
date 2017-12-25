/*---------------------------------------------------------------------------*\

  FILE........: tdma_testframer.h
  AUTHOR......: Brady O'Brien
  DATE CREATED: 11 December 2017

  Test frame generator and consumer for TDMA testing

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

#ifndef _TDMA_TEST_FRAME_H
#define _TDMA_TEST_FRAME_H

#include <stdbool.h>
#include <stdint.h>
#include "tdma.h"
#include "golay23.h"

enum tdma_tester_mode{
    tester_master,    //Tester operates as master
    tester_rxonly,    //Tester RXes only
    tester_repeater,  //Tester repeates frames
    tester_septx      
};


typedef struct {
    tdma_t * tdma;
    uint64_t nbits_rx;          //Number of rx'ed bits
    uint64_t nbits_rx_err;      //Number of rx bit errors
    uint64_t nbits_tx;          //Number of TX'ed bits
    uint32_t tx_frame_count;    //Number of TX frames
    uint32_t n_tx_slot;         //TX slot
    uint32_t rx_last_slot;      //Last slot we got a valid RX from
    uint32_t rx_last_seq;       //Last sequence number from an RX slot
    uint32_t rx_last_id;        //Last ID from an rx slot

    uint16_t tx_seq;
    uint16_t tx_id;

    bool tx_enable;             //Enable TX of frames
    bool tx_master;             //Enable TX in master mode
    bool tx_repeat;             //Repeat valid RX'ed frames
    bool print_enable;          //Report frames to stdout
} tdma_test_framer;


tdma_test_framer * ttf_create(tdma_t * tdma);
void ttf_destroy(tdma_test_framer * ttf);
void ttf_clear_counts(tdma_test_framer * ttf);

#endif
