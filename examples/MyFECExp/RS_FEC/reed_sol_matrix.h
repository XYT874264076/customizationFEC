
#ifndef REED_SOL_MATRIX_H_
#define REED_SOL_MATRIX_H_

#include <stdlib.h>
#include <bitset>

u_int8_t* reed_sol_full_coding_matrix(int n,int k,int* list);
u_int8_t* reed_sol_sub_coding_matrix(int n,int k,int* list);
u_int8_t* reed_sol_sub_stream_coding_matrix(int n,int k,int* list,int* mask,int mask_len);
u_int8_t* reed_sol_block_decoding_matrix(int n,int k,int* list, int* mask);
u_int8_t* reed_sol_stream_decoding_matrix(int n,int k,int* list, int* mask, int* k_mask);
template <std::size_t N> u_int8_t* reed_sol_block_decoding_matrix(int n,int k,int* list, std::bitset<N> &mask);
template <std::size_t N, std::size_t K> u_int8_t* reed_sol_stream_decoding_matrix(int n,int k,int* list, std::bitset<N> mask, std::bitset<K>* k_mask);
template <std::size_t N, std::size_t K> u_int8_t* reed_sol_skipped_stream_decoding_matrix(int n, int k,int* list, std::bitset<N> mask, std::bitset<N> arrive, std::bitset<K>* k_mask);

#endif // REED_SOL_MATRIX_H_