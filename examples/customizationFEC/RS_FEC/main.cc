
#include <iostream>
#include <cstdio>

#include "reed_sol_matrix.h"
#include "Galois/galois.h"

int temp_coding_list[]={95, 190, 97};
u_int8_t temp_source_value[]={5,2,0,4,6,0};
int temp_mask_list[]={1,1,0,0,1,0};
u_int8_t* code_matrix;
u_int8_t* decode_matrix;
u_int8_t* source_value;
u_int8_t* target_value;
u_int8_t* receive_value;
u_int8_t* decode_value;
int* coding_list;
int* mask_list;
int n,k,i,j;

int main(){
    n=6;
    k=3;
    coding_list=temp_coding_list;
    code_matrix=reed_sol_coding_matrix(n,k,coding_list);
    for (i=0;i<n+k;i++){
        for (j=0;j<n;j++){
            printf("%hhu\t",code_matrix[i*n+j]);
        }
        printf("\n");
    }
    printf("\n");
    source_value=temp_source_value;
    target_value=(u_int8_t*) malloc(sizeof(u_int8_t)*(n+k));
    for (i=0;i<n+k;i++) target_value[i]=0;
    for (i=0;i<n+k;i++){
        for (j=0;j<n;j++){
            target_value[i]=target_value[i]^galois_single_multiply(code_matrix[i*n+j],source_value[j],8);
        }
    }
    printf("source data:\n");
    for (i=0;i<n;i++){
        printf("%hhu ",source_value[i]);
    }
    printf("\n");
    printf("\n");
    printf("coded data:\n");
    for (i=0;i<n+k;i++){
        printf("%hhu ",target_value[i]);
    }
    printf("\n");
    printf("\n");
    mask_list=temp_mask_list;
    decode_matrix=reed_sol_block_decoding_matrix(n,k,coding_list,mask_list);
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
    for (i=0;i<n;i++){
        if (mask_list[i]==1) receive_value[i]=source_value[i];
        else {
            receive_value[i]=target_value[n+j];
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

