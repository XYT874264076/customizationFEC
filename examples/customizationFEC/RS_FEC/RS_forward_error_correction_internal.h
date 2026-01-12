
#ifndef EXAMPLES_MYFECEXP_RSFEC_RS_FORWARD_ERROR_CORRECTION_INTERNAL_H_
#define EXAMPLES_MYFECEXP_RSFEC_RS_FORWARD_ERROR_CORRECTION_INTERNAL_H_

#include <stddef.h>
#include <stdint.h>
#include <bitset>

#include "Galois/galois.h"
#include "reed_sol_matrix.h"

namespace webrtc{

constexpr size_t kRSfecMaxMediaPackets = 40;
constexpr size_t kRSfecPacketMaskSize = 4;
constexpr size_t kRSfecMagicNumSize = 2;

namespace internal {

int* GenerateBaseNumList(int n, int k);
u_int8_t* GenerateCodingMatrix(int n, int k,int* baseNumList,int* pktMask, int maskLen);
template <std::size_t N> u_int8_t* GenerateDecodingMatrix(int n, int k,int* baseNumList, std::bitset<N> mask, std::bitset<N> arrive, std::bitset<N>* k_mask);

}

}

#endif // EXAMPLES_MYFECEXP_RSFEC_RS_FORWARD_ERROR_CORRECTION_INTERNAL_H_