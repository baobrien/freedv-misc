#=
----------------------------------------------------------------------------

  FILE........: fsk_wrapper.jl
  AUTHOR......: Brady O'Brien
  DATE CREATED: 26 August 2017

  Julia wrapper for fsk.c functions

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

module cfsk
    const codec2_lib = "../freetel-code/codec2-dev/build_linux/src/libcodec2.so"
    import Base.show

    #Mirror of the C FSK modem struct
    #NOTE: This is not being used
    type fsk_modem_c
        Ndft        ::Cint
        Fs          ::Cint
        N           ::Cint
        Rs          ::Cint
        Ts          ::Cint
        Nmem        ::Cint
        P           ::Cint
        Nsym        ::Cint
        Nbits       ::Cint
        f1_tx       ::Cint
        fs_tx       ::Cint
        mode        ::Cint
        est_min     ::Cint
        est_max     ::Cint
        est_space   ::Cint
        hann_table  ::Ptr{Cfloat}
        phi_c       ::NTuple{4,Complex64}
        fft_cfg     ::Ptr{Void}
        norm_rx_timing ::Cfloat
        samp_old    ::Ptr{Complex64}
        nstash      ::Cint
        fft_est     ::Ptr{Cfloat}
        tx_phase_c  ::Complex64
        EbNodB      ::Cfloat
        f_est       ::NTuple{4,Cfloat}
        ppm         ::Cfloat
        nin         ::Cint
        stats       ::Ptr{Void}
        normalize_eye ::Cint
    end

    #Wrapper around C FSK struct
    type fsk_modem
        fsk_C::Ptr{Void}
    end

    function fsk_create_hbr(Fs,Rs,P,M,tx_f1,tx_fs)
        fsk_C = ccall(
            (:fsk_create_hbr,codec2_lib),
            Ptr{Void},
            (Cint,Cint,Cint,Cint,Cint,Cint),
            Fs,Rs,P,M,tx_f1,tx_fs
        )
        return fsk_modem(fsk_C)
    end
    
    function fsk_destroy(fsk ::fsk_modem)
        if fsk.fsk_C == C_NULL
            return
        end
        ccall(
            (:fsk_destroy,codec2_lib),
            Void,
            (Ptr{Void},),
            fsk.fsk_C)
        fsk.fsk_C = C_NULL;
    end

    function fsk_set_nsym(fsk ::fsk_modem,nsym)
        if fsk.fsk_C == C_NULL
            return;
        end
        ccall(
            (:fsk_set_nsym,codec2_lib),
            Void,
            (Ptr{Void},Cint),
            fsk.fsk_C,nsym)
    end

    #This is a hack to extract a C-int value from the structure, as julia doesn't do c-struct-from-pointer
    function fsk_get_member(fsk ::fsk_modem,T,offset)
        if fsk.fsk_C == C_NULL
            return 0;
        end
        return unsafe_load(Ptr{T}(fsk.fsk_C),offset)
    end    

    #Extract the parameter Nsym from the FSK modem
    fsk_get_nsym(fsk ::fsk_modem) = fsk_get_member(fsk,Cint,8)

    #Extract the parameter Nbits from the FSK modem
    fsk_get_nbits(fsk ::fsk_modem) = fsk_get_member(fsk,Cint,9)

    #Extract the parameter Nbits from the FSK modem
    fsk_get_N(fsk ::fsk_modem) = fsk_get_member(fsk,Cint,3)

    #Extract the number of samples per symbol
    fsk_get_Ts(fsk ::fsk_modem) = fsk_get_member(fsk,Cint,5)
        
    #Modulate a single FSK frame, produce floating point samples 
    function fsk_mod_frame(fsk ::fsk_modem,tx_bits::Array{UInt8,1})
        if fsk.fsk_C == C_NULL
            return zeros(Float32)
        end
        nbits = fsk_get_nbits(fsk)
        nsamp = fsk_get_N(fsk)
        if nbits==0 || nsamp == 0
            return zeros(Float32)
        end
        samp_array = zeros(Float32,nsamp)
        if length(tx_bits)<nbits
            pad = zeros(UInt8,nbits-length(tx_bits))
            tx_bits = [tx_bits,pad]
        end
        ccall(
            (:fsk_mod,codec2_lib),
            Void,
            (Ptr{Void},Ptr{Float32},Ptr{UInt8}),
            fsk.fsk_C,
            samp_array,
            tx_bits
        )
        return samp_array
    end        

    #Modulate a single FSK frame, produce complex samples
    function fsk_mod_c_frame(fsk ::fsk_modem,tx_bits::Array{UInt8,1})
        if fsk.fsk_C == C_NULL
            return zeros(Complex{Float32})
        end
        nbits = fsk_get_nbits(fsk)
        nsamp = fsk_get_N(fsk)
        if nbits==0 || nsamp == 0
            return zeros(Complex{Float32})
        end
        samp_array = zeros(Complex{Float32},nsamp)
        if length(tx_bits)<nbits
            pad = zeros(UInt8,nbits-length(tx_bits))
            tx_bits = [tx_bits,pad]
        end
        ccall(
            (:fsk_mod_c,codec2_lib),
            Void,
            (Ptr{Void},Ptr{Complex{Float32}},Ptr{UInt8}),
            fsk.fsk_C,
            samp_array,
            tx_bits
        )
        return samp_array
    end

    #Modulate many frames, produce float samples
    function fsk_mod(fsk ::fsk_modem,tx_bits::Array{UInt8,1})
        nbits = fsk_get_nbits(fsk)
        nsamp = fsk_get_N(fsk)
        nbitsin = length(tx_bits)        
        frames = div(nbitsin,nbits)
        sampout = zeros(Float32,frames*nsamp)
        for i = 0:frames-1
            st = i*nbits
            en = (i+1)*nbits
            frame = tx_bits[(i*nbits)+1:(i+1)*nbits]
            sampout[(i*nsamp)+1:(i+1)*nsamp] = fsk_mod_frame(fsk,tx_bits)
        end
        sampout
    end

    function fsk_mod_c(fsk ::fsk_modem,tx_bits::Array{UInt8,1})
        nbits = fsk_get_nbits(fsk)
        nsamp = fsk_get_N(fsk)
        nbitsin = length(tx_bits)        
        frames = div(nbitsin,nbits)
        sampout = zeros(Complex{Float32},frames*nsamp)
        for i = 0:frames-1
            st = i*nbits
            en = (i+1)*nbits
            frame = tx_bits[(i*nbits)+1:(i+1)*nbits]
            sampout[(i*nsamp)+1:(i+1)*nsamp] = fsk_mod_c_frame(fsk,tx_bits)
        end
        sampout
    end
end
