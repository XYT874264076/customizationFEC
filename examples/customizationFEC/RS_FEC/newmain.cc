
#include <iostream>
#include <cstdio>
#include <bitset>

#include "reed_sol_matrix.h"
#include "Galois/galois.h"

int temp_coding_list1[]={95};
int temp_coding_list2[]={190, 97};
int temp_coding_list[]={95,190,97};
u_int8_t temp_source_value_1[]={5,2,0};
u_int8_t temp_source_value_2[]={0,4,6,0};
u_int8_t temp_source_value[]={5,2,0,4,6,0};
std::bitset<6> mask_full("110001");
std::bitset<6> mask_one("111000");
std::bitset<6> mask_two("001111");
std::bitset<6> mask_three("001111");
std::bitset<6> mask_list[3];
u_int8_t* code_matrix;
u_int8_t* decode_matrix;
u_int8_t* source_value;
u_int8_t* fec_value;
u_int8_t* target_value;
u_int8_t* receive_value;
u_int8_t* decode_value;
int* coding_list1,*coding_list2;
int n1,n2,n_full,k1,k2,k_full,i,j,fec_i;
int n,k;

int main(){
    n1=3;
    n2=4;
    k1=1;
    k2=2;
    fec_i=0;
    n=6;
    k=3;
    fec_value=(u_int8_t*) malloc(sizeof(u_int8_t)*(n2+k1));

    coding_list1=temp_coding_list1;
    code_matrix=reed_sol_coding_matrix(n1,k1,coding_list1);
    for (i=0;i<n1+k1;i++){
        for (j=0;j<n1;j++){
            printf("%hhu\t",code_matrix[i*n1+j]);
        }
        printf("\n");
    }
    printf("\n");
    source_value=temp_source_value_1;
    target_value=(u_int8_t*) malloc(sizeof(u_int8_t)*(n1+k1));
    for (i=0;i<n1+k1;i++) target_value[i]=0;
    for (i=0;i<n1+k1;i++){
        for (j=0;j<n1;j++){
            target_value[i]=target_value[i]^galois_single_multiply(code_matrix[i*n1+j],source_value[j],8);
        }
    }
    printf("source data:\n");
    for (i=0;i<n1;i++){
        printf("%hhu ",source_value[i]);
    }
    printf("\n");
    printf("\n");
    printf("coded data:\n");
    for (i=0;i<n1+k1;i++){
        printf("%hhu ",target_value[i]);
    }
    printf("\n");
    printf("\n");
    for (i=n1;i<n1+k1;i++){
        fec_value[fec_i]=target_value[i];
        fec_i++;
    }

    coding_list2=temp_coding_list2;
    code_matrix=reed_sol_coding_matrix(n2,k2,coding_list2);
    for (i=0;i<n2+k2;i++){
        for (j=0;j<n2;j++){
            printf("%hhu\t",code_matrix[i*n2+j]);
        }
        printf("\n");
    }
    printf("\n");
    source_value=temp_source_value_2;
    target_value=(u_int8_t*) malloc(sizeof(u_int8_t)*(n2+k2));
    for (i=0;i<n2+k2;i++) target_value[i]=0;
    for (i=0;i<n2+k2;i++){
        for (j=0;j<n2;j++){
            target_value[i]=target_value[i]^galois_single_multiply(code_matrix[i*n2+j],source_value[j],8);
        }
    }
    printf("source data:\n");
    for (i=0;i<n2;i++){
        printf("%hhu ",source_value[i]);
    }
    printf("\n");
    printf("\n");
    printf("coded data:\n");
    for (i=0;i<n2+k2;i++){
        printf("%hhu ",target_value[i]);
    }
    printf("\n");
    printf("\n");
    for (i=n2;i<n2+k2;i++){
        fec_value[fec_i]=target_value[i];
        fec_i++;
    }

    mask_list[0]=mask_one;
    mask_list[1]=mask_two;
    mask_list[2]=mask_three;
    coding_list1=temp_coding_list;

    decode_matrix=reed_sol_stream_decoding_matrix(n,k,coding_list1,mask_full,mask_list);
    if (decode_matrix==NULL){
        printf("decode error!\n");
        return -1;
    }
    for (i=0;i<n;i++){
        for (j=0;j<n;j++){
            printf("%hhu\t",decode_matrix[i*n+j]);
        }
        printf("\n");
    }
    printf("\n");
    printf("\n");
    receive_value=(u_int8_t*) malloc(sizeof(u_int8_t)*(n));
    printf("received data:\n");
    j=0;
    source_value=temp_source_value;
    for (i=0;i<n;i++){
        if (mask_full[n-1-i]==1) receive_value[i]=source_value[i];
        else {
            receive_value[i]=fec_value[j];
            j++;
        }
    }
    for (i=0;i<n;i++){
        printf("%hhu ",receive_value[i]);
    }
    printf("\n");
    printf("\n");
    decode_value=(u_int8_t*) malloc(sizeof(u_int8_t)*(n));
    for (i=0;i<n+k;i++) decode_value[i]=0;
    for (i=0;i<n;i++){
        for (j=0;j<n;j++){
            decode_value[i]=decode_value[i]^galois_single_multiply(decode_matrix[i*n+j],receive_value[j],8);
        }
    }
    printf("decoded data:\n");
    for (i=0;i<n;i++){
        printf("%hhu ",decode_value[i]);
    }
    printf("\n");
}

