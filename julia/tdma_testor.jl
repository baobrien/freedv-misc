#=
----------------------------------------------------------------------------

  FILE........: tdma_testor.jl
  AUTHOR......: Brady O'Brien
  DATE CREATED: 16 September 2017

  Testbench for TDMA modem

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

module tdmatestor
    include("tdma_channel_sim.jl")
    include("tdma_wrapper.jl")

    function main()
        srand(10)
        nsets = 30 # Number of complete slot periods

        #Set up a modem
        modem = ctdma.tdma_create(ctdma.FREEDV_4800T)

        #Make some TDMA
        sim = tdmasim.tdma_sim_rand(tdmasim.config_4800T)
        sim.xmitters[2].EbN0 = 2;
        sim.xmitters[1].timing_offset = -35;
        sim.xmitters[2].timing_offset = 35;
        sim.xmitters[1].freq_offset = 1000;
        sim.xmitters[1].master = true;
        sim.xmitters[2].master = false;
        #sim.xmitters[1].enable = false;
        tdma_rf = tdmasim.tdma_sim_run(sim,nsets)
        n_start_noise = 200
        start_noise = convert(Array{Float32},randn(n_start_noise)) + im*convert(Array{Float32},randn(n_start_noise))
        tdma_rf = [start_noise;tdma_rf]
        #Demod some TDMA
        ctdma.tdma_rx_abunch(modem,tdma_rf,0)

        outfile = open("out.cf32","w+")
        write(outfile,tdma_rf)
        close(outfile)
    end
end