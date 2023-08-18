#!/bin/bash
if [ $# -eq 0 ];
then
    echo "What Jesd [0 to 15] to debug?"
    read jesd_idx
    echo "Is it Rx or Tx ?"
    read xcvr
else
    jesd_idx=$1
    xcvr=$2
fi

if [ $# -eq 1 ] || [ $# -gt 2 ];
then
    echo "Please provide 1- Jesd Idx (0-16)      2- Rx or Tx"
    echo "end"
    exit 1
fi

if [ $jesd_idx -gt 15 ];
then
    echo "JESD IP index doesn't exists"
    echo "  (JESD IP 0 - 15)"
    echo "end"
    exit 1
fi

if [[ $xcvr == *"R"* ]] || [[ $xcvr == *"r"* ]];
then
    jesd_rx=1
    jesd_tx=0
    max_index=17
else
    jesd_rx=0
    jesd_tx=1
    max_index=8
fi

jesd_core_onehot=$((1 << $jesd_idx))
jesd_core_onehot_hex=$(echo "obase=16; $jesd_core_onehot" | bc)
#echo "jesd_core_onehot     = $jesd_core_onehot"
#echo "jesd_core_onehot_hex = $jesd_core_onehot_hex"

mem rw net6 $jesd_core_onehot_hex;  mem rw net7 0x18;  mem rw net9 0x1; mem rw net9 0x0;
rx_err0=$(mem rr res_ro30)
tx_err=$(mem rr res_ro30)
mem rw net6 $jesd_core_onehot_hex;  mem rw net7 0x19;  mem rw net9 0x1; mem rw net9 0x0;
rx_err1=$(mem rr res_ro30)

if [ $jesd_rx -eq 1 ];
then
    echo "Debugging JESD $jesd_idx (RX)"
    echo "rx_err0 = $rx_err0"
    echo "rx_err1 = $rx_err1"
else
    echo "Debugging JESD $jesd_idx (TX)"
    echo "tx_err = $tx_err"
fi

#RX:
csr_pcfifo_empty_err=$(($rx_err0 & 0x80))
csr_pcfifo_full_err=$(($rx_err0 & 0x40))
csr_rx_locked_to_data_err=$(($rx_err0 & 0x20))
csr_lane_deskew_err=$(($rx_err0 & 0x10))
csr_frame_data_ready_err=$(($rx_err0 & 0x8))
csr_dll_data_ready_err=$(($rx_err0 & 0x4))
csr_sysref_lmfc_err=$(($rx_err0 & 0x2))

csr_ecc_fatal_err=$(($rx_err1 & 0x200))
casr_ecc_corrected_err=$(($rx_err1 & 0x100))
dlerrs_rs=$(($rx_err1 & 0x80))
csr_ilas_err=$(($rx_err1 & 0x40))
csr_disparity_err=$(($rx_err1 & 0x20))
csr_not_in_table_err=$(($rx_err1 & 0x10))
csr_unexpected_kchar=$(($rx_err1 & 0x8))
csr_lane_alignment_err=$(($rx_err1 & 0x4))
csr_frame_alignment_err=$(($rx_err1 & 0x2))
csr_cg_sync_err=$(($rx_err1 & 0x1))

#TX:
csr_pcfifo_empty_err=$(($tx_err & 0x80))
csr_pcfifo_full_err=$(($tx_err & 0x40))
csr_pll_locked_err=$(($tx_err & 0x20))
csr_syncn_reinit_req=$(($tx_err & 0x10))
csr_frame_data_invalid_err=$(($tx_err & 0x8))
csr_dll_data_invalid_err=$(($tx_err & 0x4))
csr_sysref_lmfc_err=$(($tx_err & 0x2))
csr_syncn_err=$(($tx_err & 0x1))

declare -A ErrorArray
declare -A ErrorArrayName
 
#RX:
ErrorArray[0]=$csr_pcfifo_empty_err
ErrorArray[1]=$csr_pcfifo_full_err
ErrorArray[2]=$csr_rx_locked_to_data_err
ErrorArray[3]=$csr_lane_deskew_err
ErrorArray[4]=$csr_frame_data_ready_err
ErrorArray[5]=$csr_dll_data_ready_err
ErrorArray[6]=$csr_sysref_lmfc_err
ErrorArray[7]=$csr_ecc_fatal_err
ErrorArray[8]=$casr_ecc_corrected_err
ErrorArray[9]=$dlerrs_rs
ErrorArray[10]=$csr_ilas_err
ErrorArray[11]=$csr_disparity_err
ErrorArray[12]=$csr_not_in_table_err
ErrorArray[13]=$csr_unexpected_kchar
ErrorArray[14]=$csr_lane_alignment_err
ErrorArray[15]=$csr_frame_alignment_err
ErrorArray[16]=$csr_cg_sync_err
ErrorArrayName[0]="csr_pcfifo_empty_err"
ErrorArrayName[1]="csr_pcfifo_full_err"
ErrorArrayName[2]="csr_rx_locked_to_data_err"
ErrorArrayName[3]="csr_lane_deskew_err"
ErrorArrayName[4]="csr_frame_data_ready_err"
ErrorArrayName[5]="csr_dll_data_ready_err"
ErrorArrayName[6]="csr_sysref_lmfc_err"
ErrorArrayName[7]="csr_ecc_fatal_err"
ErrorArrayName[8]="casr_ecc_corrected_err"
ErrorArrayName[9]="dlerrs_rs"
ErrorArrayName[10]="csr_ilas_err"
ErrorArrayName[11]="csr_disparity_err"
ErrorArrayName[12]="csr_not_in_table_err"
ErrorArrayName[13]="csr_unexpected_kchar"
ErrorArrayName[14]="csr_lane_alignment_err"
ErrorArrayName[15]="csr_frame_alignment_err"
ErrorArrayName[16]="csr_cg_sync_err"


declare -A ErrorArrayTx
declare -A ErrorArrayTxName
#TX:
ErrorArrayTx[0]=$csr_pcfifo_empty_err
ErrorArrayTx[1]=$csr_pcfifo_full_err
ErrorArrayTx[2]=$csr_pll_locked_err
ErrorArrayTx[3]=$csr_syncn_reinit_req
ErrorArrayTx[4]=$csr_frame_data_invalid_err
ErrorArrayTx[5]=$csr_dll_data_invalid_err
ErrorArrayTx[6]=$csr_sysref_lmfc_err
ErrorArrayTx[7]=$csr_syncn_err
ErrorArrayTxName[0]="csr_pcfifo_empty_err"
ErrorArrayTxName[1]="csr_pcfifo_full_err"
ErrorArrayTxName[2]="csr_pll_locked_err"
ErrorArrayTxName[3]="csr_syncn_reinit_req"
ErrorArrayTxName[4]="csr_frame_data_invalid_err"
ErrorArrayTxName[5]="csr_dll_data_invalid_err"
ErrorArrayTxName[6]="csr_sysref_lmfc_err"
ErrorArrayTxName[7]="csr_syncn_err"
ErrorArrayTxDescription[0]="1 or more lanes of Phase Compensation FIFO was empty when link was running"
ErrorArrayTxDescription[1]="1 or more lanes of Phase Compensation FIFO was full when link was running."
ErrorArrayTxDescription[2]="1 or more lanes of PLL locked lost lock when link was running."
ErrorArrayTxDescription[3]="Received has requested reinit by asserting SYNC_N low for more than 5 frames and 9 octets."
ErrorArrayTxDescription[4]="AVS LINK DATA VALID is low (must always be high)"
ErrorArrayTxDescription[5]="AVS LINK DATA VALID is low (must always be high)"
ErrorArrayTxDescription[6]="SYSREF period doesn't match the LMF counter"
ErrorArrayTxDescription[7]="JESD Receiver indicates error through the SYNC_N signal"

#echo "csr_unexpected_kchar = $csr_unexpected_kchar"
#echo "csr_cg_sync_err = $csr_cg_sync_err"

error_detected=0
echo "             "
echo "ERRORS detected :"
for ((bit=0; bit<$max_index; bit++))
do
#    echo "bit = $bit"
    if [ $jesd_rx -eq 1 ];
    then
        if [ ${ErrorArray[$bit]} -ne 0 ];
        then
        error_detected=1
        echo "  - ${ErrorArrayName[$bit]}"
        fi
    else
        if [ ${ErrorArrayTx[$bit]} -ne 0 ];
        then
        error_detected=1
        echo "  - ${ErrorArrayTxName[$bit]}"
        echo "        ${ErrorArrayTxDescription[$bit]}"
        fi
    fi
done

reset_errors=0
if [ $error_detected -ne 0 ];  
then
    echo " "
    echo " Do you want to reset the errors? 1 for Yes, 0 for No"
    read reset_errors 
else
    echo " No errors detected, link should be UP."
fi


if [ $reset_errors -eq 1 ];
then
    if [ $jesd_rx -eq 1 ];
    then
        mem rw net6 $jesd_core_onehot_hex;  mem rw net7 0x18; mem rw net8 $rx_err0 ; mem rw net9 0x2; mem rw net9 0x0;
        mem rw net6 $jesd_core_onehot_hex;  mem rw net7 0x19; mem rw net8 $rx_err1 ; mem rw net9 0x2; mem rw net9 0x0;
    else
        mem rw net6 $jesd_core_onehot_hex;  mem rw net7 0x18; mem rw net8 $tx_err  ; mem rw net9 0x2; mem rw net9 0x0;
    fi
fi

echo "Done"




######select JESD_0 
#####mem rw net6 0x1;
#####
######Select address: rx_err0 = 0x18   rx_err1 = 0x19
#####mem rw net7 0x18;
#####
######issue a read enable
#####mem rw net9 0x1; mem rw net9 0x0;
#####
######read:
#####mem rr res_ro30;
#####
######select JESD_0 
#####mem rw net6 0x1;  mem rw net7 0x18;  mem rw net9 0x1; mem rw net9 0x0; mem rr res_ro30;
#####mem rw net6 0x1;  mem rw net7 0x19;  mem rw net9 0x1; mem rw net9 0x0; mem rr res_ro30;



