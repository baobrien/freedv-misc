module tdmasim
    include("fsk_wrapper.jl")
    #using cfsk

    #Structure for keeping track of 'transmitters'
    type TdmaXmtr    
        fsk ::cfsk.fsk_modem    #The modem itself
        timing_offset ::Int32   #Timing offset in samples
        EbN0 ::Float32          #AWGN Channel EbN0
        freq_offset ::Float32   #Frequency offset
        master                  #Flag to indicate master status. Currently does nothing.
    end

    #Structure containing high level config for simulation
    type TdmaConfig
        symrate     #modem symbol rate
        samprate    #modem/simulation sample rate
        slot_syms   #bits per full slot (including dead padding)
        frame_syms  #'live' bits in frame
        fsk_m       #number of FSK tones
        slots       #N-slots per tdma system simulation
    end

    #Struct containing the actual TDMA sim locals
    type TdmaSim
        xmitters ::Array{TdmaXmtr,1}
        config ::TdmaConfig
    end

    #FreeDV 4800T configuration
    const config_4800T = TdmaConfig(2400,48000,48,44,4,2)

    #Tenative TDMA unique word
    const uw = UInt8[0,1,1,0,0,1,1,1,1,0,1,0,1,1,0,1]

    function setup_tdma_sim(config ::TdmaConfig)
        #Setup slot xmitters
        Fs = config.samprate
        Rs = config.symrate
        M = config.fsk_m
        TdmaSim(
            map(x->(
                fsk = cfsk.fsk_create_hbr(Fs,Rs,(Fs/Rs),M,Rs,Rs);
                cfsk.fsk_set_nsym(fsk,config.frame_syms);
                TdmaXmtr(fsk,0,0.,0.,false))
                ,1:config.slots),
            config
        )
    end

    #Generate some bits for an FSK frame
    #Right now just generate some random bits and stick a UW in
    #Master will eventually be a boolean indicating the master xmttr, right now does nothing
    function generate_frame_bits(sim,master)
        nsyms = sim.config.frame_syms
        nbits = Int(log2(sim.config.fsk_m))*nsyms
        bits = [UInt8(x?1:0) for x in rand(Bool,nbits)]
        uwoffset = div(nbits,2) - div(length(uw),2)
        #Insert UW
        bits[uwoffset+1:uwoffset+length(uw)] = uw
        return bits
    end
    
    #Generate a slot's worth of modulated and impaired fsk
    function modulate_slot(sim,xmtr)
        #Inter-frame padding (in symbols)
        padding = (sim.config.slot_syms - sim.config.frame_syms)
        #Padding on the front and back of frame
        padding = div(padding,2)
        Ts = cfsk.fsk_get_Ts(xmtr.fsk)
        nslotsyms = sim.config.slot_syms
        nsyms = sim.config.frame_syms
        padding_samps = Ts*padding
        assert(abs(xmtr.timing_offset)<padding_samps)

        EbN0_Scale = 1

        println("slot:$nslotsyms frame:$nsyms\n")

        frame_bits = generate_frame_bits(sim,xmtr.master)
        samps = zeros(Complex{Float32},nslotsyms*Ts)
        fsk_sig = cfsk.fsk_mod_c(xmtr.fsk,frame_bits)

        frame_offset = padding_samps+xmtr.timing_offset

        samps[(1:Ts*nsyms)+frame_offset] = fsk_sig*EbN0_Scale
        samps
    end

    function tdma_sim_main()

    end

    function mainish()
        sim = setup_tdma_sim(config_4800T)
        sim.xmitters[1].timing_offset=-35
        modulate_slot(sim,sim.xmitters[1])
    end

end

tdmasim.mainish()