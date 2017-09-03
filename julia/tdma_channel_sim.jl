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

    #Set up a TDMA simulation from a configuration
    function TdmaSim(config ::TdmaConfig)
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
    function modulate_slot(sim::TdmaSim,xmtr::TdmaXmtr)
        #Inter-frame padding (in symbols)
        padding = (sim.config.slot_syms - sim.config.frame_syms)
        #Padding on the front and back of frame
        padding = div(padding,2)
        Ts = cfsk.fsk_get_Ts(xmtr.fsk)
        nslotsyms = sim.config.slot_syms
        nsyms = sim.config.frame_syms
        padding_samps = Ts*padding
        assert(abs(xmtr.timing_offset)<padding_samps)

        #Scale this signal for Eb/N0
        #TODO: figure out how to do this
        EbN0_Scale = 1

        #Frequency shift
        w = 2.*pi*xmtr.freq_offset/Float32(sim.config.samprate)
        shift = exp(im*(1:nsyms*Ts)*w)

        frame_bits = generate_frame_bits(sim,xmtr.master)
        samps = zeros(Complex{Float32},nslotsyms*Ts)
        fsk_sig = cfsk.fsk_mod_c(xmtr.fsk,frame_bits)
        fsk_sig = fsk_sig .* shift
        frame_offset = padding_samps+xmtr.timing_offset

        samps[(1:Ts*nsyms)+frame_offset] = fsk_sig*EbN0_Scale
        samps
    end

    function randrange(min::Int64 ,max::Int64)
        range = max-min
        rangeb2 = Integer(2.^ceil(log2(range)))-1
        num = rand(Int64)&rangeb2
        num <= range ? num+min : randrange(min,max)
    end

    function tdma_sim_main(config,nsets,outfile)
        sim = TdmaSim(config)
        #Seed the RNG
        srand(1)
        range_ebno = (8.,18.)
        range_freq = (-300.,300.)
        range_timing = (-25,25)
        #Give some random impairments
        for slot = sim.xmitters
            slot.EbN0 = range_ebno[1] + rand()*(range_ebno[2]-range_ebno[1])
            slot.freq_offset = range_freq[1] + rand()*(range_freq[2]-range_freq[1])
            slot.timing_offset = randrange(range_timing[1],range_timing[2])
        end
        Ts = div(sim.config.samprate,sim.config.symrate)
        nsamps = nsets*sim.config.slots*sim.config.slot_syms*Ts

        sampbuf = Complex{Float32}[]
        for i=0:nsets-1
            st = i    *(config.slot_syms*Ts)
            en = (i+1)*(config.slot_syms*Ts)
            for slot = sim.xmitters
                #Modulate slots and build up sample buffer progressively
                #NOTE: this could be a bottleneck at some point
                sampbuf = vcat(sampbuf,modulate_slot(sim,slot))
            end
        end
        
        #make some noise
        noise_i = convert(Array{Float32},randn(nsamps))
        noise_r = convert(Array{Float32},randn(nsamps))
        #Add constant noise to signal
        sampbuf += (noise_r .+ noise_i .* im).*.1
    end

    function mainish()
        sim = TdmaSim(config_4800T)
        sim.xmitters[1].timing_offset=-35
        sim.xmitters[1].freq_offset=4800
        samps = modulate_slot(sim,sim.xmitters[1])
        noise_i = convert(Array{Float32},randn(length(samps)))
        noise_r = convert(Array{Float32},randn(length(samps)))
        noise = noise_i.*im + noise_r

        samps+noise*.1
    end

end

#tdmasim.mainish()