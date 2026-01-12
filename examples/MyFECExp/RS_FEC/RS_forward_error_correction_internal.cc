
#include "examples/MyFECExp/RS_FEC/RS_forward_error_correction_internal.h" 

#include <string.h>
#include <algorithm>
#include <iostream>

#include "rtc_base/logging.h"

namespace webrtc {

namespace internal {

size_t curBaseNum = 1;
size_t baseMulNum = 2;

int* GenerateBaseNumList(int n, int k){

    if ((size_t)n>kRSfecMaxMediaPackets){
        RTC_LOG(LS_ERROR) << "GenerateBaseNumList Error: N can't over kRSfecMaxMediaPackets which is 32.";
        return NULL;
    }

    int i;
    int* dist;
    dist = (int*) malloc(sizeof(int)*k);
    for (i=0;i<k;i++){
        dist[i]=curBaseNum;
        curBaseNum = galois_single_multiply(curBaseNum,baseMulNum,8);
    }
    
    return dist;

}

u_int8_t* GenerateCodingMatrix(int n, int k,int* baseNumList,int* pktMask, int maskLen){

    u_int8_t* dist;

    dist=reed_sol_sub_stream_coding_matrix(n, k, baseNumList, pktMask, maskLen);

    return dist;

}

template u_int8_t* GenerateDecodingMatrix<128ul>(int, int, int*, std::bitset<128ul>, std::bitset<128ul>, std::bitset<128ul>*);

template <std::size_t N> u_int8_t* GenerateDecodingMatrix(int n, int k, int* baseNumList, std::bitset<N> mask, std::bitset<N> arrive, std::bitset<N>* k_mask) {
    
    u_int8_t* dist;

    dist = reed_sol_skipped_stream_decoding_matrix(n, k, baseNumList, mask, arrive, k_mask);

    return dist;
}

}

}