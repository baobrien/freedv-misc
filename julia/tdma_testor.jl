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
        nsets = 5 # Number of complete slot periods

        #Set up a modem
        modem = ctdma.tdma_create(ctdma.FREEDV_4800T)

        #Make some TDMA
        sim = tdmasim.tdma_sim_rand(tdmasim.config_4800T)
        #sim.xmitters[1].EbN0 = -1;
        sim.xmitters[1].timing_offset = -20;
        sim.xmitters[2].timing_offset = 35;
        sim.xmitters[1].freq_offset = 1000;
        tdma_rf = tdmasim.tdma_sim_run(sim,nsets)
        #Demod some TDMA
        ctdma.tdma_rx_abunch(modem,tdma_rf,0)
    end
end