//
// Copyright 2014 - 2022 Per Vices Corporation
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#pragma once

//channels.h contains code for determining how many channels to use
//TODO: edit code so server.c and property manager so that it does not require channels.h, make nrnt not require channels.h
#include "../channels.h"

#if defined(R8) && defined(T0)

    #define S_NUM_RX "8"
    #define S_NUM_TX "0"

    #define INDIVIDUAL_RESET_BIT_OFFSET_RX 4
    #define INDIVIDUAL_RESET_BIT_OFFSET_TX 0

    // LR (left right) slot number for channel (using numbering where the number increases left to right first, before going down, different from RFE slot number)
    // Special case: 4R4T 3G uses the same numbering as 4R4T 1G even though the physical row is lower
    #define LR_NUM_RX(ch) (INT(ch))
    #define LR_NUM_TX(ch) 0


    static const char *rx_sfp_map[NUM_RX_CHANNELS] = { "sfpa", "sfpa", "sfpb", "sfpb", "sfpc", "sfpc", "sfpd", "sfpd" };

    static const char *rx_ip_dst[NUM_RX_CHANNELS] = { "10.10.10.10", "10.10.10.10", "10.10.11.10", "10.10.11.10", "10.10.12.10", "10.10.12.10", "10.10.13.10", "10.10.13.10" };

    static const int rx_jesd_map[NUM_RX_CHANNELS] = { 0, 1, 0, 1, 0, 1, 0, 1 };

    // Register (in device_side_port_map) for desination port for tx, only applies when using DDR
    // See the comment for device_side_port_map in properties_tate_nrnt.c for a more detailed explaination
    static const int tx_dst_port_map[NUM_TX_CHANNELS] = { };

    //contains the registers used for the general purpose register for each channel
    //most registers follow the pattern rxa0 for ch a, rxb0 for ch b, the general purpose register for each channel does not
    static const char *rx_reg4_map[NUM_RX_CHANNELS] = { "rxa4", "rxb4", "rxe4", "rxf4", "rxi4", "rxj4", "rxm4", "rxn4" };
    static const char *tx_reg4_map[NUM_TX_CHANNELS] = { };

    //registers used for trigger streaming
    //note: these registers have multiple purposes and only masked writes should be done to them
    //at time of writing it is per sfp, not per channel, hence the overlap
    static const char *rx_trig_sel_map[NUM_RX_CHANNELS] = { "rxa9", "rxa9", "rxb9", "rxb9", "rxc9", "rxc9", "rxd9", "rxd9"};
    static const char *rx_trig_sma_mode_map[NUM_RX_CHANNELS] = { "rxa9", "rxa9", "rxb9", "rxb9", "rxc9", "rxc9", "rxd9", "rxd9"};
    static const char *rx_trig_ufl_mode_map[NUM_RX_CHANNELS] = { "rxa9", "rxa9", "rxb9", "rxb9", "rxc9", "rxc9", "rxd9", "rxd9"};

    static const char *tx_trig_sel_map[NUM_TX_CHANNELS] = {  };
    static const char *tx_trig_sma_mode_map[NUM_TX_CHANNELS] = {  };
    static const char *tx_trig_ufl_mode_map[NUM_TX_CHANNELS] = {  };

    static const char *tx_nsamp_msw_map[NUM_TX_CHANNELS] = {  };
    static const char *tx_nsamp_lsw_map[NUM_TX_CHANNELS] = {  };

#elif defined(R4) && defined(T4)

    #define S_NUM_RX "4"
    #define S_NUM_TX "4"

    #define INDIVIDUAL_RESET_BIT_OFFSET_RX 4
    #define INDIVIDUAL_RESET_BIT_OFFSET_TX 12

    // LR (left right) slot number for channel (using numbering where the number increases left to right first, before going down, different from RFE slot number)
    // Special case: 4R4T 3G uses the same numbering as 4R4T 1G even though the physical row is lower
    #define LR_NUM_RX(ch) (INT(ch))
    #define LR_NUM_TX(ch) (INT(ch) + 8)


    static const char *rx_sfp_map[NUM_RX_CHANNELS] = { "sfpa", "sfpb", "sfpc", "sfpd" };

    static const char *tx_sfp_map[NUM_TX_CHANNELS] = { "sfpa", "sfpb", "sfpc", "sfpd" };

    static const char *rx_ip_dst[NUM_RX_CHANNELS] = { "10.10.10.10", "10.10.11.10", "10.10.12.10", "10.10.13.10" };

    static const int rx_jesd_map[NUM_RX_CHANNELS] = { 0, 0, 0, 0 };

    // Register (in device_side_port_map) for desination port for tx, only applies when using DDR
    // See the comment for device_side_port_map in properties_tate_nrnt.c for a more detailed explaination
    static const int tx_dst_port_map[NUM_TX_CHANNELS] = { 0, 4, 8, 12 };

    //contains the registers used for the general purpose register for each channel
    //most registers follow the pattern rxa0 for ch a, rxb0 for ch b, the general purpose register for each channel does not
    static const char *rx_reg4_map[NUM_RX_CHANNELS] = { "rxa4", "rxe4", "rxi4", "rxm4" };
    static const char *tx_reg4_map[NUM_TX_CHANNELS] = { "txa4", "txb4", "txc4", "txd4" };

    //registers used for trigger streaming
    //note: these registers have multiple purposes and only masked writes should be done to them
    //at time of writing it is per sfp, not per channel, hence the overlap
    static const char *rx_trig_sel_map[NUM_RX_CHANNELS] = { "rxa9", "rxb9", "rxc9", "rxd9"};
    static const char *rx_trig_sma_mode_map[NUM_RX_CHANNELS] = { "rxa9", "rxb9", "rxc9", "rxd9"};
    static const char *rx_trig_ufl_mode_map[NUM_RX_CHANNELS] = { "rxa9", "rxb9", "rxc9", "rxd9"};

    static const char *tx_trig_sel_map[NUM_TX_CHANNELS] = { "txi6", "txj6", "txk6", "txl6" };
    static const char *tx_trig_sma_mode_map[NUM_TX_CHANNELS] = { "txi6", "txj6", "txk6", "txl6" };
    static const char *tx_trig_ufl_mode_map[NUM_TX_CHANNELS] = { "txi6", "txj6", "txk6", "txl6" };

    static const char *tx_nsamp_msw_map[NUM_TX_CHANNELS] = { "txi7", "txj7", "txk7", "txl7" };
    static const char *tx_nsamp_lsw_map[NUM_TX_CHANNELS] = { "txi8", "txj8", "txk8", "txl8" };

    //least significant 32 bits used to store underflow count
    static const char *tx_uflow_map_lsb[NUM_TX_CHANNELS] = { "flc6", "flc8", "flc10", "flc12" };
    //most significant 32 bits used to store underflow count
    static const char *tx_uflow_map_msb[NUM_TX_CHANNELS] = { "flc7", "flc9", "flc11", "flc13" };
    //least significant 32 bits used to store overflow count
    static const char *tx_oflow_map_lsb[NUM_TX_CHANNELS] = { "flc14", "flc16", "flc18", "flc20" };
    //most significant 32 bits used to store overflow count
    static const char *tx_oflow_map_msb[NUM_TX_CHANNELS] = { "flc15", "flc17", "flc19", "flc21" };

#elif defined(R9) && defined(T7)

    #define S_NUM_RX "9"
    #define S_NUM_TX "7"

    #define INDIVIDUAL_RESET_BIT_OFFSET_RX 4
    #define INDIVIDUAL_RESET_BIT_OFFSET_TX 13

    // LR (left right) slot number for channel (using numbering where the number increases left to right first, before going down, different from RFE slot number)
    // Special case: 4R4T 3G uses the same numbering as 4R4T 1G even though the physical row is lower
    #define LR_NUM_RX(ch) (INT(ch))
    #define LR_NUM_TX(ch) (INT(ch) + 9)

    static const char *rx_sfp_map[NUM_RX_CHANNELS] = { "sfpa", "sfpa", "sfpb", "sfpb", "sfpc", "sfpc", "sfpd", "sfpd", "sfpd" };
    static const char *tx_sfp_map[NUM_TX_CHANNELS] = { "sfpa", "sfpb", "sfpb", "sfpc", "sfpc", "sfpd", "sfpd" };

    static const char *rx_ip_dst[NUM_RX_CHANNELS] = { "10.10.10.10", "10.10.10.10", "10.10.11.10", "10.10.11.10", "10.10.12.10", "10.10.12.10", "10.10.13.10", "10.10.13.10", "10.10.13.10" };

    static const int rx_jesd_map[NUM_RX_CHANNELS] = { 0, 1, 0, 1, 0, 1, 0, 1, 2 };

    // Register (in device_side_port_map) for desination port for tx, only applies when using DDR
    // See the comment for device_side_port_map in properties_tate_nrnt.c for a more detailed explaination
    static const int tx_dst_port_map[NUM_TX_CHANNELS] = { 0, 4, 5, 8, 9, 12, 13};

    //contains the registers used for the general purpose register for each channel
    //most registers follow the pattern rxa0 for ch a, rxb0 for ch b, the general purpose register for each channel does not
    static const char *rx_reg4_map[NUM_RX_CHANNELS] = { "rxa4", "rxb4", "rxe4", "rxf4", "rxi4", "rxj4", "rxm4", "rxn4", "rxo4" };
    static const char *tx_reg4_map[NUM_TX_CHANNELS] = { "txa4", "txb4", "txc4", "txd4", "txe4", "txf4", "txg4" };

    //registers used for trigger streaming
    //note: these registers have multiple purposes and only masked writes should be done to them
    //at time of writing it is per sfp, not per channel, hence the overlap
    static const char *rx_trig_sel_map[NUM_RX_CHANNELS] = { "rxa9", "rxa9", "rxb9", "rxb9", "rxc9", "rxc9", "rxd9", "rxd9", "rxd9"};
    static const char *rx_trig_sma_mode_map[NUM_RX_CHANNELS] = { "rxa9", "rxa9", "rxb9", "rxb9", "rxc9", "rxc9", "rxd9", "rxd9", "rxd9"};
    static const char *rx_trig_ufl_mode_map[NUM_RX_CHANNELS] = { "rxa9", "rxa9", "rxb9", "rxb9", "rxc9", "rxc9", "rxd9", "rxd9", "rxd9"};

    static const char *tx_trig_sel_map[NUM_TX_CHANNELS] = { "txj6", "txk6", "txl6", "txm6", "txn6", "txo6", "txp6" };
    static const char *tx_trig_sma_mode_map[NUM_TX_CHANNELS] = { "txj6", "txk6", "txl6", "txm6", "txn6", "txo6", "txp6" };
    static const char *tx_trig_ufl_mode_map[NUM_TX_CHANNELS] = { "txj6", "txk6", "txl6", "txm6", "txn6", "txo6", "txp6" };

    static const char *tx_nsamp_msw_map[NUM_TX_CHANNELS] = { "txj7", "txk7", "txl7", "txm7", "txn7", "txo7", "txp7"};
    static const char *tx_nsamp_lsw_map[NUM_TX_CHANNELS] = { "txj8", "txk8", "txl8", "txm8", "txn8", "txo8", "txp8"};

    //least significant 32 bits used to store underflow count
    static const char *tx_uflow_map_lsb[NUM_TX_CHANNELS] = { "flc6", "flc8", "flc10", "flc12", "flc44", "flc46", "flc48" };
    //most significant 32 bits used to store underflow count
    static const char *tx_uflow_map_msb[NUM_TX_CHANNELS] = { "flc7", "flc9", "flc11", "flc13", "flc45", "flc47", "flc49" };
    //least significant 32 bits used to store overflow count
    static const char *tx_oflow_map_lsb[NUM_TX_CHANNELS] = { "flc14", "flc16", "flc18", "flc20", "flc52", "flc54", "flc56" };
    //most significant 32 bits used to store overflow count
    static const char *tx_oflow_map_msb[NUM_TX_CHANNELS] = { "flc15", "flc17", "flc19", "flc21", "flc53", "flc55", "flc57" };

#else
    #error Invalid channel config, only 4R4T, 9R7T, and 8R0T have been implemented
#endif