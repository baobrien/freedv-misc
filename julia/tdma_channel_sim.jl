module tdmasim
    include("fsk_wrapper.jl")
    #using cfsk

    #Structure for keeping track of 'transmitters'
    type TdmaXmtr    
        fsk ::cfsk.fsk_modem    #The modem itself
        timing_offset ::Int32   #Timing offset in samples
        EbN0 ::Float32          #AWGN Channel EbN0
        freq_offset ::Float32   #Frequency offset

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

    const config_4800T = TdmaConfig(2400,48000,48,44,4,2)

    function setup_tdma_sim(config ::TdmaConfig)
        #Setup slot xmitters
        Fs = config.samprate
        Rs = config.symrate
        M = config.fsk_m
        TdmaSim(
            map(x->(
                fsk = cfsk.fsk_create_hbr(Fs,Rs,(Fs/Rs),M,Rs,Rs);
                cfsk.fsk_set_nsym(fsk,config.frame_syms);
                TdmaXmtr(fsk,0,0.,0.))
                ,1:config.slots),
            config
        )
    end

    function tdma_sim_main()

    end

end