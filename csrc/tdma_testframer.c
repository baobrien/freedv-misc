/*---------------------------------------------------------------------------*\

  FILE........: tdma_testframer.c
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

#include "tdma_testframer.h"

static int ttf_tx_frame(u8* frame_bits,u32 slot_i, slot_t * slot, tdma_t * tdma,u8 * uw_type, void * cb_data){
    tdma_test_framer * ttf = (tdma_test_framer*) cb_data;

    if(ttf == NULL)
        return 0;

    if(!ttf->tx_enable){
        tdma_stop_tx(tdma,slot_i);
        return 0;
    }

    if(ttf->tx_master){
        frame_bits[tdma->master_bit_pos] = 1;
        tdma->state = master_sync;
    }else{
        frame_bits[tdma->master_bit_pos] = 0;
        tdma->state = rx_no_sync;
    }

    /* TODO: implement repeater mode */
    if(ttf->tx_repeat){
        return 0;
    }

    ttf->tx_seq += 1;
    if(ttf->tx_seq >= 4096)
        ttf->tx_seq = 0;

    int tx_seq_enc = golay23_encode((uint32_t)(ttf->tx_seq&0xFFF));
    int tx_id_enc = golay23_encode((uint32_t)(ttf->tx_id&0xFFF));

    ttf->nbits_tx += 23*2;

    int bit_idx = 0;
    int word_idx = 0;
    for(;bit_idx<23;bit_idx++,word_idx++){
        frame_bits[bit_idx] = (tx_seq_enc>>word_idx)&1;
    }

    word_idx = 0;
    bit_idx = 62;
    for(;word_idx<23;bit_idx++,word_idx++){
        frame_bits[bit_idx] = (tx_id_enc>>word_idx)&1;
    }

    *uw_type = 1;

    return 1;
}

static void ttf_rx_frame(u8* frame_bits,u32 slot_i, slot_t * slot, tdma_t * tdma,u8 uw_type, void * cb_data){
    tdma_test_framer * ttf = (tdma_test_framer*) cb_data;
    if(ttf == NULL)
        return;

    int bit_idx = 0;
    int word_idx = 0;
    int rx_seq_enc = 0;
    int rx_id_enc = 0;

    for(;bit_idx<23;bit_idx++,word_idx++){
        rx_seq_enc |= (frame_bits[bit_idx]?1:0)<<word_idx;
    }

    word_idx = 0;
    bit_idx = 62;
    for(;word_idx<23;bit_idx++,word_idx++){
        rx_id_enc |= (frame_bits[bit_idx]?1:0)<<word_idx;
    }

    int rx_seq_dec = golay23_decode(rx_seq_enc);
    int rx_id_dec = golay23_decode(rx_id_enc);

    int errs = golay23_count_errors(rx_seq_dec,rx_seq_enc);
    errs += golay23_count_errors(rx_id_enc,rx_id_dec);

    uint16_t rx_seq = (uint16_t)(rx_seq_dec>>11);
    uint16_t rx_id  = (uint16_t)(rx_id_dec>>11);
    ttf->rx_last_id = rx_id;
    ttf->rx_last_seq = rx_seq;
    ttf->rx_last_slot = slot_i;
    ttf->nbits_rx += 23*2;
    ttf->nbits_rx_err += errs;

    if(ttf->print_enable)
        fprintf(stdout,"Got Frame seq %d id %d errs %d\n",(int)rx_seq,(int)rx_id,errs);    
}

tdma_test_framer * ttf_create(tdma_t * tdma){
    tdma_test_framer * ttf = malloc(sizeof(tdma_test_framer));
    golay23_init();
    /* Setup callbacks */
    tdma_set_rx_cb(tdma,ttf_rx_frame,(void*)ttf);
    tdma_set_tx_cb(tdma,ttf_tx_frame,(void*)ttf);
    ttf->tdma = tdma;
    
    return ttf;
}

void ttf_destroy(tdma_test_framer * ttf){
    if(ttf == NULL)
        return;
    
    /* Clear callbacks */
    tdma_set_rx_cb(ttf->tdma,NULL,NULL);
    tdma_set_tx_cb(ttf->tdma,NULL,NULL);

    free(ttf);
}

void ttf_clear_counts(tdma_test_framer * ttf){
    if(ttf == NULL)
        return;

    ttf->rx_last_id = 0;
    ttf->rx_last_seq = 0;
    ttf->rx_last_slot = 0;
    ttf->nbits_rx = 0;
    ttf->nbits_rx_err = 0;
    ttf->nbits_tx = 0;
    ttf->tx_frame_count = 0;
}