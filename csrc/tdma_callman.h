/*---------------------------------------------------------------------------*\

  FILE........: tdma_callman.h
  AUTHOR......: Brady O'Brien
  DATE CREATED: 11 December 2017

  TDMA call framer/manager for 4800T

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

#ifndef __TDMA_CALLMAN_H
#define __TDMA_CALLMAN_H

#include "tdma.h"
#include <stdint.h>
#include <stdbool.h>

enum frame_type{
    frame_type_voice,   //Voice channel frame
    frame_type_datach,  //Data channel frame
    frame_type_socf,    //Start of call frame
    frame_type_eocf,    //End of call frame
    frame_type_dburst,  //Data burst (not part of ongoing call)
};

enum call_type{
    call_type_voice,    //Voice+Data call
    call_type_data      //Data channel only call
};

typedef struct callerinfo_s{
    uint64_t saddr;     //48 bit station address
    uint16_t idhash;    //16-bit hash of address and name(?)
    uint8_t name[32];   //Station callsign+name in UTF-8
    bool name_valid;    //Valid name flag
    bool addr_valid;    //Valid address flag

} callerinfo_t;

typedef struct callinfo_s{
    callerinfo_t * caller;  //Who made the call
    uint8_t cid;            //ID of call in progress
    uint64_t icid;          //Internal call ID -- to keep track of all uniquie calls
    enum call_type ctype;   //Type of call
    uint8_t * cpacketbuf;   //Buffer of packet being constructed
    uint32_t cpacketlen;    //Length of packing being constructed
} callinfo_t;

#endif