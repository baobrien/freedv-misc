#=
----------------------------------------------------------------------------

  FILE........: tdma_wrapper.jl
  AUTHOR......: Brady O'Brien
  DATE CREATED: 5 september 2017

  Julia wrapper for the tdma.c modem

----------------------------------------------------------------------------
=#

#=
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
=#

module ctdma
    const codec2_lib = "../freetel-code/codec2-dev/build_linux/src/libcodec2.so"
    import Base.show

    #Wrapper around TDMA settings struct (struct TDMA_MODE_SETTINGS in tdma.h)
    type tdma_mode_settings
        sym_rate            ::UInt32
        fsk_m               ::UInt32
        samp_rate           ::UInt32
        slot_size           ::UInt32
        frame_size          ::UInt32
        n_slots             ::UInt32
        frame_type          ::UInt32
        uw_len              ::UInt32
        pilot_sync_tol      ::UInt32
        first_sync_tol      ::UInt32
        frame_sync_tol      ::UInt32
        baduw_tol           ::UInt32
        mastersat_max       ::UInt32
        mastersat_min       ::UInt32
        slot_desync_frames  ::UInt32
    end

    const FREEDV_VHF_FRAME_AT = 3;
    const FREEDV_4800T = tdma_mode_settings(2400,4,48000,48,44,2,FREEDV_VHF_FRAME_AT,16,2,2,2,2,6,3,5)

    type tdma_modem
        tdma_C ::Ptr{Void}
        mode ::tdma_mode_settings
    end

    function tdma_create(mode ::tdma_mode_settings)
        tdma_C = ccall(
            (:tdma_create,codec2_lib),
            Ptr{Void},
            (tdma_mode_settings,),
            mode
        )
        if tdma_C == C_NULL
            throw(ErrorException("Unknown problem creating TDMA modem"))
        end
        modem = tdma_modem(tdma_C,mode)
        finalizer(modem,tdma_destroy) #hook tdma_destroy into Julia's GC
        return modem
    end

    function tdma_destroy(tdma ::tdma_modem)
        ccall(
            (:tdma_destroy,codec2_lib),
            Void,
            (Ptr{Void},),
            tdma.tdma_C
        )
    end

    function tdma_print_stuff(tdma ::tdma_modem)
        ccall(
            (:tdma_print_stuff,codec2_lib),
            Void,
            (Ptr{Void},),
            tdma.tdma_C
        )
    end

    function tdma_rx(tdma ::tdma_modem,samps ::Array{Complex64,1}, timestamp ::Int64 )
        Ts = tdma.mode.samp_rate / tdma.mode.sym_rate
        slot_samps = tdma.mode.slot_size * Ts
        assert( length(samps) == slot_samps )
        ccall(
            (:tdma_rx,codec2_lib),
            Void,
            (Ptr{Void},Ptr{Complex64},Int64),
            tdma.tdma_C,
            samps,
            timestamp
        )
    end
    function tdma_rx_abunch(tdma ::tdma_modem,samps ::Array{Complex64,1}, timestamp ::Int64)
        Ts = div(tdma.mode.samp_rate, tdma.mode.sym_rate)
        slot_samps = tdma.mode.slot_size * Ts
        assert(length(samps)>0)
        #assert(length(samps)%slot_samps == 0)
        chunks = div(length(samps),slot_samps)
        ts = timestamp
        for ii = 0:chunks-1
            st = Int(ii*slot_samps)
            en = Int((ii+1)*slot_samps)
            tdma_rx(tdma,samps[st+1:en],ts)
            ts+=slot_samps
        end
    end
    
    #typedef void (*tdma_cb_rx_frame)(u8* frame_bits,u32 slot_i, slot_t * slot, tdma_t * tdma, void * cb_data);
    #function tdma_rx_callback(frame_bits ::Array{UInt8}, slot_i ::UInt32, slot ::Ptr{Void},tdma ::Ptr{Void}, cb_data ::Ptr{Void})
        
    #end
end