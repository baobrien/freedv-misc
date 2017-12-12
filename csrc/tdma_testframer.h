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

enum tdma_tester_mode{
    tester_master,
    tester_rxonly,
    tester_repeater,
    
};

struct tdma_test_framer {
    tdma_t * tdma;
    uint64_t nbits_rx;          //Number of rx'ed bits
    uint64_t nbits_rx_err;      //Number of rx bit errors
    uint64_t nbits_tx;
    uint32_t tx_frame_count;
    uint32_t n_tx_slot;         //TX slot

    bool tx_enable;
    bool tx_master;
};

#endif
