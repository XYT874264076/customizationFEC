
#include <iostream>

#include "reed_sol_matrix.h"
#include "Galois/galois.h"

u_int8_t* reed_sol_full_coding_matrix(int n,int k,int* list){
    int i,j;
    u_int8_t* vdm;
    int rindex, tmp, rows, cols, mul_num;

    rows = n+k;
    cols = n;

    vdm = (u_int8_t*) malloc(sizeof(u_int8_t) * rows * cols);
    if (vdm == NULL) return NULL;

    for (i=0;i<cols;i++){
        for (j=0;j<cols;j++){
            if (i==j) vdm[i*cols+j]=1;
            else vdm[i*cols+j]=0;
        }
    }

    for (i=0;i<k;i++){
        rindex=i+cols;
        tmp=1;
        mul_num=list[i];
        for (j=0;j<cols;j++){
            vdm[rindex*cols+j]=tmp;
            tmp=galois_single_multiply(tmp, mul_num, 8);
        }
    }

    return vdm;
}

u_int8_t* reed_sol_sub_coding_matrix(int n,int k,int* list){
    int i,j;
    u_int8_t* vdm;
    int rindex, tmp, rows, cols, mul_num;

    rows = k;
    cols = n;

    vdm = (u_int8_t*) malloc(sizeof(u_int8_t) * rows * cols);
    if (vdm == NULL) return NULL;

    for (i=0;i<k;i++){
        rindex=i;
        tmp=1;
        mul_num=list[i];
        for (j=0;j<cols;j++){
            vdm[rindex*cols+j]=tmp;
            tmp=galois_single_multiply(tmp, mul_num, 8);
        }
    }

    return vdm;
}

u_int8_t* reed_sol_sub_stream_coding_matrix(int n,int k,int* list,int* mask,int mask_len){
    int i,j;
    u_int8_t* vdm;
    int rindex, tmp, rows, cols, mul_num, mask_id;

    rows = k;
    cols = n;

    vdm = (u_int8_t*) malloc(sizeof(u_int8_t) * rows * cols);
    if (vdm == NULL) return NULL;

    for (i=0;i<k;i++){
        rindex=i;
        tmp=1;
        mul_num=list[i];
        mask_id=0;
        for (j=0;j<cols;j++){
            while ((mask[mask_id]&1)==0) {
                mask_id++;
                if (mask_id>=mask_len){
                    if (vdm != NULL) free(vdm);
                    return NULL;
                }
                tmp=galois_single_multiply(tmp, mul_num,8);
            }
            vdm[rindex*cols+j]=tmp;
            mask_id++;
            tmp=galois_single_multiply(tmp, mul_num, 8);
        }
    }

    return vdm;
}

u_int8_t* reed_sol_block_decoding_matrix(int n,int k,int* list, int* mask){
    int i,j;
    uint8_t* vdm;
    uint8_t* dist;
    int rindex, tmp, rows, cols, mul_num, list_i, now_s, now_r;
    int s_i,r_i,from_i,to_i;
    int* s_list;
    int* r_list;
    int* v_list;

    rows = n;
    cols = n*2;
    list_i=0;
    now_r=0;
    now_s=0;

    vdm = (u_int8_t*) malloc(sizeof(uint8_t) * rows * cols);
    dist = (u_int8_t*) malloc(sizeof(uint8_t) * n * n);
    s_list = (int*) malloc(sizeof(int)*n);
    r_list = (int*) malloc(sizeof(int)*k);
    v_list = (int*) malloc(sizeof(int)*k);
    if (vdm == NULL || dist == NULL || s_list == NULL || r_list == NULL || v_list == NULL){
        if (vdm!=NULL) free(vdm);
        if (dist!=NULL) free(dist);
        if (s_list!=NULL) free(s_list);
        if (r_list!=NULL) free(r_list);
        if (v_list!=NULL) free(v_list);
        return NULL;
    } 
    for (i=0;i<n;i++){
        if ((mask[i] & 1) == 0){
            if (list_i>=k) {
                free(vdm);
                free(dist);
                free(s_list);
                free(r_list);
                free(v_list);
                return NULL;
            }
            mul_num=list[list_i];
            list_i++;
            tmp=1;
            rindex=i;
            for (j=0;j<n;j++){
                vdm[rindex*cols+j]=tmp;
                tmp=galois_single_multiply(tmp, mul_num, 8);
            }
            r_list[now_r]=i;
            v_list[now_r]=mul_num;
            now_r++;
        }
        else {
            for (j=0;j<n;j++){
                if (i==j) vdm[i*cols+j]=1;
                else vdm[i*cols+j]=0;
            }
            s_list[now_s]=i;
            now_s++;
        }   
    }
    
    for (i=0;i<n;i++){
        for (j=0;j<n;j++){
            if (i==j) vdm[i*cols+n+j]=1;
            else vdm[i*cols+n+j]=0;
        }
    }

    // Get inverse matrix
    for (s_i=0;s_i<now_s;s_i++){
        from_i=s_list[s_i];
        for (r_i=0;r_i<now_r;r_i++){
            to_i=r_list[r_i];
            j=from_i;
            mul_num=vdm[to_i*cols+j];
            vdm[to_i*cols+j]=vdm[to_i*cols+j]^mul_num;
            vdm[to_i*cols+n+j]=vdm[to_i*cols+n+j]^mul_num;
        }
    }

    for (r_i=0;r_i<now_r;r_i++){
        from_i=r_list[r_i];
        if (vdm[from_i*cols+from_i]!=1){
            if (vdm[from_i*cols+from_i]==0){

            }
            tmp = galois_single_divide(1, vdm[from_i*cols+from_i], 8);
            for (j=0;j<cols;j++){
                vdm[from_i*cols+j]=galois_single_multiply(vdm[from_i*cols+j], tmp, 8);
            }
        }

        for (s_i=r_i+1;s_i<now_r;s_i++){
            to_i=r_list[s_i];
            mul_num=vdm[to_i*cols+from_i];
            for (j=0;j<cols;j++){
                tmp=galois_single_multiply(vdm[from_i*cols+j],mul_num, 8);
                vdm[to_i*cols+j]=vdm[to_i*cols+j]^tmp;
            }
        }
    }

    for (r_i=now_r-1;r_i>=0;r_i--){
        from_i=r_list[r_i];
        if (vdm[from_i*cols+from_i]!=1){
            tmp = galois_single_divide(1, vdm[from_i*cols+from_i], 8);
            for (j=0;j<cols;j++){
                vdm[from_i*cols+j]=galois_single_multiply(vdm[from_i*cols+j], tmp, 8);
            }
        }

        for (s_i=r_i-1;s_i>=0;s_i--){
            to_i=r_list[s_i];
            mul_num=vdm[to_i*cols+from_i];
            for (j=0;j<cols;j++){
                tmp=galois_single_multiply(vdm[from_i*cols+j],mul_num, 8);
                vdm[to_i*cols+j]=vdm[to_i*cols+j]^tmp;
            }
        }
    }

    for (i=0;i<rows;i++){
        for (j=0;j<n;j++){
            dist[i*n+j]=vdm[i*cols+n+j];
        }
    }

    free(vdm);
    free(s_list);
    free(r_list);
    free(v_list);
    return (dist);
}

u_int8_t* reed_sol_stream_decoding_matrix(int n,int k,int* list, int* mask, int* k_mask){
    int i,j;
    uint8_t* vdm;
    uint8_t* dist;
    int rindex, tmp, rows, cols, mul_num, list_i, now_s, now_r;
    int s_i,r_i,from_i,to_i;
    int* s_list;
    int* r_list;
    int* v_list;

    rows = n;
    cols = n*2;
    list_i=0;
    now_r=0;
    now_s=0;

    vdm = (u_int8_t*) malloc(sizeof(uint8_t) * rows * cols);
    dist = (u_int8_t*) malloc(sizeof(uint8_t) * n * n);
    s_list = (int*) malloc(sizeof(int)*n);
    r_list = (int*) malloc(sizeof(int)*k);
    v_list = (int*) malloc(sizeof(int)*k);
    if (vdm == NULL || dist == NULL || s_list == NULL || r_list == NULL || v_list == NULL){
        if (vdm!=NULL) free(vdm);
        if (dist!=NULL) free(dist);
        if (s_list!=NULL) free(s_list);
        if (r_list!=NULL) free(r_list);
        if (v_list!=NULL) free(v_list);
        return NULL;
    } 
    for (i=0;i<n;i++){
        if ((mask[i] & 1) == 0){
            if (list_i>=k) {
                free(vdm);
                free(dist);
                free(s_list);
                free(r_list);
                free(v_list);
                return NULL;
            }
            mul_num=list[list_i];
            tmp=1;
            rindex=i;
            for (j=0;j<n;j++){
                if ((k_mask[list_i*n+j]&1)==1){
                    vdm[rindex*cols+j]=tmp;
                    tmp=galois_single_multiply(tmp, mul_num, 8);
                }
                else {
                    vdm[rindex*cols+j]=0;
                }
            }
            r_list[now_r]=i;
            v_list[now_r]=mul_num;
            now_r++;
            list_i++;
        }
        else {
            for (j=0;j<n;j++){
                if (i==j) vdm[i*cols+j]=1;
                else vdm[i*cols+j]=0;
            }
            s_list[now_s]=i;
            now_s++;
        }   
    }
    
    for (i=0;i<n;i++){
        for (j=0;j<n;j++){
            if (i==j) vdm[i*cols+n+j]=1;
            else vdm[i*cols+n+j]=0;
        }
    }

    // Get inverse matrix
    for (s_i=0;s_i<now_s;s_i++){
        from_i=s_list[s_i];
        for (r_i=0;r_i<now_r;r_i++){
            to_i=r_list[r_i];
            j=from_i;
            mul_num=vdm[to_i*cols+j];
            vdm[to_i*cols+j]=vdm[to_i*cols+j]^mul_num;
            vdm[to_i*cols+n+j]=vdm[to_i*cols+n+j]^mul_num;
        }
    }

    for (r_i=0;r_i<now_r;r_i++){
        from_i=r_list[r_i];
        if (vdm[from_i*cols+from_i]!=1){
            if (vdm[from_i*cols+from_i]==0){

            }
            tmp = galois_single_divide(1, vdm[from_i*cols+from_i], 8);
            for (j=0;j<cols;j++){
                vdm[from_i*cols+j]=galois_single_multiply(vdm[from_i*cols+j], tmp, 8);
            }
        }

        for (s_i=r_i+1;s_i<now_r;s_i++){
            to_i=r_list[s_i];
            mul_num=vdm[to_i*cols+from_i];
            for (j=0;j<cols;j++){
                tmp=galois_single_multiply(vdm[from_i*cols+j],mul_num, 8);
                vdm[to_i*cols+j]=vdm[to_i*cols+j]^tmp;
            }
        }
    }

    for (r_i=now_r-1;r_i>=0;r_i--){
        from_i=r_list[r_i];
        if (vdm[from_i*cols+from_i]!=1){
            tmp = galois_single_divide(1, vdm[from_i*cols+from_i], 8);
            for (j=0;j<cols;j++){
                vdm[from_i*cols+j]=galois_single_multiply(vdm[from_i*cols+j], tmp, 8);
            }
        }

        for (s_i=r_i-1;s_i>=0;s_i--){
            to_i=r_list[s_i];
            mul_num=vdm[to_i*cols+from_i];
            for (j=0;j<cols;j++){
                tmp=galois_single_multiply(vdm[from_i*cols+j],mul_num, 8);
                vdm[to_i*cols+j]=vdm[to_i*cols+j]^tmp;
            }
        }
    }

    for (i=0;i<rows;i++){
        for (j=0;j<n;j++){
            dist[i*n+j]=vdm[i*cols+n+j];
        }
    }

    free(vdm);
    free(s_list);
    free(r_list);
    free(v_list);
    return (dist);
}

template <std::size_t N> u_int8_t* reed_sol_block_decoding_matrix(int n,int k,int* list, std::bitset<N> &mask){
    int i,j;
    uint8_t* vdm;
    uint8_t* dist;
    int rindex, tmp, rows, cols, mul_num, list_i, now_s, now_r;
    int s_i,r_i,from_i,to_i;
    int* s_list;
    int* r_list;
    int* v_list;

    rows = n;
    cols = n*2;
    list_i=0;
    now_r=0;
    now_s=0;

    vdm = (u_int8_t*) malloc(sizeof(uint8_t) * rows * cols);
    dist = (u_int8_t*) malloc(sizeof(uint8_t) * n * n);
    s_list = (int*) malloc(sizeof(int)*n);
    r_list = (int*) malloc(sizeof(int)*k);
    v_list = (int*) malloc(sizeof(int)*k);
    if (vdm == NULL || dist == NULL || s_list == NULL || r_list == NULL || v_list == NULL){
        if (vdm!=NULL) free(vdm);
        if (dist!=NULL) free(dist);
        if (s_list!=NULL) free(s_list);
        if (r_list!=NULL) free(r_list);
        if (v_list!=NULL) free(v_list);
        return NULL;
    } 
    for (i=0;i<n;i++){
        if (mask[n-1-i] == 0){
            if (list_i>=k) {
                free(vdm);
                free(dist);
                free(s_list);
                free(r_list);
                free(v_list);
                return NULL;
            }
            mul_num=list[list_i];
            list_i++;
            tmp=1;
            rindex=i;
            for (j=0;j<n;j++){
                vdm[rindex*cols+j]=tmp;
                tmp=galois_single_multiply(tmp, mul_num, 8);
            }
            r_list[now_r]=i;
            v_list[now_r]=mul_num;
            now_r++;
        }
        else {
            for (j=0;j<n;j++){
                if (i==j) vdm[i*cols+j]=1;
                else vdm[i*cols+j]=0;
            }
            s_list[now_s]=i;
            now_s++;
        }   
    }
    
    for (i=0;i<n;i++){
        for (j=0;j<n;j++){
            if (i==j) vdm[i*cols+n+j]=1;
            else vdm[i*cols+n+j]=0;
        }
    }

    // Get inverse matrix
    for (s_i=0;s_i<now_s;s_i++){
        from_i=s_list[s_i];
        for (r_i=0;r_i<now_r;r_i++){
            to_i=r_list[r_i];
            j=from_i;
            mul_num=vdm[to_i*cols+j];
            vdm[to_i*cols+j]=vdm[to_i*cols+j]^mul_num;
            vdm[to_i*cols+n+j]=vdm[to_i*cols+n+j]^mul_num;
        }
    }

    for (r_i=0;r_i<now_r;r_i++){
        from_i=r_list[r_i];
        if (vdm[from_i*cols+from_i]!=1){
            if (vdm[from_i*cols+from_i]==0){

            }
            tmp = galois_single_divide(1, vdm[from_i*cols+from_i], 8);
            for (j=0;j<cols;j++){
                vdm[from_i*cols+j]=galois_single_multiply(vdm[from_i*cols+j], tmp, 8);
            }
        }

        for (s_i=r_i+1;s_i<now_r;s_i++){
            to_i=r_list[s_i];
            mul_num=vdm[to_i*cols+from_i];
            for (j=0;j<cols;j++){
                tmp=galois_single_multiply(vdm[from_i*cols+j],mul_num, 8);
                vdm[to_i*cols+j]=vdm[to_i*cols+j]^tmp;
            }
        }
    }

    for (r_i=now_r-1;r_i>=0;r_i--){
        from_i=r_list[r_i];
        if (vdm[from_i*cols+from_i]!=1){
            tmp = galois_single_divide(1, vdm[from_i*cols+from_i], 8);
            for (j=0;j<cols;j++){
                vdm[from_i*cols+j]=galois_single_multiply(vdm[from_i*cols+j], tmp, 8);
            }
        }

        for (s_i=r_i-1;s_i>=0;s_i--){
            to_i=r_list[s_i];
            mul_num=vdm[to_i*cols+from_i];
            for (j=0;j<cols;j++){
                tmp=galois_single_multiply(vdm[from_i*cols+j],mul_num, 8);
                vdm[to_i*cols+j]=vdm[to_i*cols+j]^tmp;
            }
        }
    }

    for (i=0;i<rows;i++){
        for (j=0;j<n;j++){
            dist[i*n+j]=vdm[i*cols+n+j];
        }
    }

    free(vdm);
    free(s_list);
    free(r_list);
    free(v_list);
    return (dist);
}

template u_int8_t* reed_sol_stream_decoding_matrix<1ul, 1ul>(int, int, int*, std::bitset<1ul>, std::bitset<1ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<2ul, 2ul>(int, int, int*, std::bitset<2ul>, std::bitset<2ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<3ul, 3ul>(int, int, int*, std::bitset<3ul>, std::bitset<3ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<4ul, 4ul>(int, int, int*, std::bitset<4ul>, std::bitset<4ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<5ul, 5ul>(int, int, int*, std::bitset<5ul>, std::bitset<5ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<6ul, 6ul>(int, int, int*, std::bitset<6ul>, std::bitset<6ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<7ul, 7ul>(int, int, int*, std::bitset<7ul>, std::bitset<7ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<8ul, 8ul>(int, int, int*, std::bitset<8ul>, std::bitset<8ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<9ul, 9ul>(int, int, int*, std::bitset<9ul>, std::bitset<9ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<10ul, 10ul>(int, int, int*, std::bitset<10ul>, std::bitset<10ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<11ul, 11ul>(int, int, int*, std::bitset<11ul>, std::bitset<11ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<12ul, 12ul>(int, int, int*, std::bitset<12ul>, std::bitset<12ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<13ul, 13ul>(int, int, int*, std::bitset<13ul>, std::bitset<13ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<14ul, 14ul>(int, int, int*, std::bitset<14ul>, std::bitset<14ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<15ul, 15ul>(int, int, int*, std::bitset<15ul>, std::bitset<15ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<16ul, 16ul>(int, int, int*, std::bitset<16ul>, std::bitset<16ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<17ul, 17ul>(int, int, int*, std::bitset<17ul>, std::bitset<17ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<18ul, 18ul>(int, int, int*, std::bitset<18ul>, std::bitset<18ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<19ul, 19ul>(int, int, int*, std::bitset<19ul>, std::bitset<19ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<20ul, 20ul>(int, int, int*, std::bitset<20ul>, std::bitset<20ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<21ul, 21ul>(int, int, int*, std::bitset<21ul>, std::bitset<21ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<22ul, 22ul>(int, int, int*, std::bitset<22ul>, std::bitset<22ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<23ul, 23ul>(int, int, int*, std::bitset<23ul>, std::bitset<23ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<24ul, 24ul>(int, int, int*, std::bitset<24ul>, std::bitset<24ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<25ul, 25ul>(int, int, int*, std::bitset<25ul>, std::bitset<25ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<26ul, 26ul>(int, int, int*, std::bitset<26ul>, std::bitset<26ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<27ul, 27ul>(int, int, int*, std::bitset<27ul>, std::bitset<27ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<28ul, 28ul>(int, int, int*, std::bitset<28ul>, std::bitset<28ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<29ul, 29ul>(int, int, int*, std::bitset<29ul>, std::bitset<29ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<30ul, 30ul>(int, int, int*, std::bitset<30ul>, std::bitset<30ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<31ul, 31ul>(int, int, int*, std::bitset<31ul>, std::bitset<31ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<32ul, 32ul>(int, int, int*, std::bitset<32ul>, std::bitset<32ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<33ul, 33ul>(int, int, int*, std::bitset<33ul>, std::bitset<33ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<34ul, 34ul>(int, int, int*, std::bitset<34ul>, std::bitset<34ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<35ul, 35ul>(int, int, int*, std::bitset<35ul>, std::bitset<35ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<36ul, 36ul>(int, int, int*, std::bitset<36ul>, std::bitset<36ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<37ul, 37ul>(int, int, int*, std::bitset<37ul>, std::bitset<37ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<38ul, 38ul>(int, int, int*, std::bitset<38ul>, std::bitset<38ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<39ul, 39ul>(int, int, int*, std::bitset<39ul>, std::bitset<39ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<40ul, 40ul>(int, int, int*, std::bitset<40ul>, std::bitset<40ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<41ul, 41ul>(int, int, int*, std::bitset<41ul>, std::bitset<41ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<42ul, 42ul>(int, int, int*, std::bitset<42ul>, std::bitset<42ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<43ul, 43ul>(int, int, int*, std::bitset<43ul>, std::bitset<43ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<44ul, 44ul>(int, int, int*, std::bitset<44ul>, std::bitset<44ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<45ul, 45ul>(int, int, int*, std::bitset<45ul>, std::bitset<45ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<46ul, 46ul>(int, int, int*, std::bitset<46ul>, std::bitset<46ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<47ul, 47ul>(int, int, int*, std::bitset<47ul>, std::bitset<47ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<48ul, 48ul>(int, int, int*, std::bitset<48ul>, std::bitset<48ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<49ul, 49ul>(int, int, int*, std::bitset<49ul>, std::bitset<49ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<50ul, 50ul>(int, int, int*, std::bitset<50ul>, std::bitset<50ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<51ul, 51ul>(int, int, int*, std::bitset<51ul>, std::bitset<51ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<52ul, 52ul>(int, int, int*, std::bitset<52ul>, std::bitset<52ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<53ul, 53ul>(int, int, int*, std::bitset<53ul>, std::bitset<53ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<54ul, 54ul>(int, int, int*, std::bitset<54ul>, std::bitset<54ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<55ul, 55ul>(int, int, int*, std::bitset<55ul>, std::bitset<55ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<56ul, 56ul>(int, int, int*, std::bitset<56ul>, std::bitset<56ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<57ul, 57ul>(int, int, int*, std::bitset<57ul>, std::bitset<57ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<58ul, 58ul>(int, int, int*, std::bitset<58ul>, std::bitset<58ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<59ul, 59ul>(int, int, int*, std::bitset<59ul>, std::bitset<59ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<60ul, 60ul>(int, int, int*, std::bitset<60ul>, std::bitset<60ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<61ul, 61ul>(int, int, int*, std::bitset<61ul>, std::bitset<61ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<62ul, 62ul>(int, int, int*, std::bitset<62ul>, std::bitset<62ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<63ul, 63ul>(int, int, int*, std::bitset<63ul>, std::bitset<63ul>*);
template u_int8_t* reed_sol_stream_decoding_matrix<64ul, 64ul>(int, int, int*, std::bitset<64ul>, std::bitset<64ul>*);

template <std::size_t N, std::size_t K> u_int8_t* reed_sol_stream_decoding_matrix(int n,int k,int* list, std::bitset<N> mask, std::bitset<K>* k_mask){
    int i,j;
    uint8_t* vdm;
    uint8_t* dist;
    int rindex, tmp, rows, cols, mul_num, list_i, now_s, now_r;
    int s_i,r_i,from_i,to_i;
    int* s_list;
    int* r_list;
    int* v_list;

    rows = n;
    cols = n*2;
    list_i=0;
    now_r=0;
    now_s=0;

    vdm = (u_int8_t*) malloc(sizeof(uint8_t) * rows * cols);
    dist = (u_int8_t*) malloc(sizeof(uint8_t) * n * n);
    s_list = (int*) malloc(sizeof(int)*n);
    r_list = (int*) malloc(sizeof(int)*k);
    v_list = (int*) malloc(sizeof(int)*k);
    if (vdm == NULL || dist == NULL || s_list == NULL || r_list == NULL || v_list == NULL){
        if (vdm!=NULL) free(vdm);
        if (dist!=NULL) free(dist);
        if (s_list!=NULL) free(s_list);
        if (r_list!=NULL) free(r_list);
        if (v_list!=NULL) free(v_list);
        return NULL;
    } 
    for (i=0;i<n;i++){
        if (mask[n-1-i] == 0){
            if (list_i>=k) {
                free(vdm);
                free(dist);
                free(s_list);
                free(r_list);
                free(v_list);
                return NULL;
            }
            mul_num=list[list_i];
            tmp=1;
            rindex=i;
            for (j=0;j<n;j++){
                if (k_mask[list_i][n-1-j] == 1){
                    vdm[rindex*cols+j]=tmp;
                    tmp=galois_single_multiply(tmp, mul_num, 8);
                }
                else {
                    vdm[rindex*cols+j]=0;
                }
            }
            r_list[now_r]=i;
            v_list[now_r]=mul_num;
            now_r++;
            list_i++;
        }
        else {
            for (j=0;j<n;j++){
                if (i==j) vdm[i*cols+j]=1;
                else vdm[i*cols+j]=0;
            }
            s_list[now_s]=i;
            now_s++;
        }   
    }
    
    for (i=0;i<n;i++){
        for (j=0;j<n;j++){
            if (i==j) vdm[i*cols+n+j]=1;
            else vdm[i*cols+n+j]=0;
        }
    }

    // Get inverse matrix
    for (s_i=0;s_i<now_s;s_i++){
        from_i=s_list[s_i];
        for (r_i=0;r_i<now_r;r_i++){
            to_i=r_list[r_i];
            j=from_i;
            mul_num=vdm[to_i*cols+j];
            vdm[to_i*cols+j]=vdm[to_i*cols+j]^mul_num;
            vdm[to_i*cols+n+j]=vdm[to_i*cols+n+j]^mul_num;
        }
    }

    for (r_i=0;r_i<now_r;r_i++){
        from_i=r_list[r_i];
        if (vdm[from_i*cols+from_i]!=1){
            if (vdm[from_i*cols+from_i]==0){

            }
            tmp = galois_single_divide(1, vdm[from_i*cols+from_i], 8);
            for (j=0;j<cols;j++){
                vdm[from_i*cols+j]=galois_single_multiply(vdm[from_i*cols+j], tmp, 8);
            }
        }

        for (s_i=r_i+1;s_i<now_r;s_i++){
            to_i=r_list[s_i];
            mul_num=vdm[to_i*cols+from_i];
            for (j=0;j<cols;j++){
                tmp=galois_single_multiply(vdm[from_i*cols+j],mul_num, 8);
                vdm[to_i*cols+j]=vdm[to_i*cols+j]^tmp;
            }
        }
    }

    for (r_i=now_r-1;r_i>=0;r_i--){
        from_i=r_list[r_i];
        if (vdm[from_i*cols+from_i]!=1){
            tmp = galois_single_divide(1, vdm[from_i*cols+from_i], 8);
            for (j=0;j<cols;j++){
                vdm[from_i*cols+j]=galois_single_multiply(vdm[from_i*cols+j], tmp, 8);
            }
        }

        for (s_i=r_i-1;s_i>=0;s_i--){
            to_i=r_list[s_i];
            mul_num=vdm[to_i*cols+from_i];
            for (j=0;j<cols;j++){
                tmp=galois_single_multiply(vdm[from_i*cols+j],mul_num, 8);
                vdm[to_i*cols+j]=vdm[to_i*cols+j]^tmp;
            }
        }
    }

    for (i=0;i<rows;i++){
        for (j=0;j<n;j++){
            dist[i*n+j]=vdm[i*cols+n+j];
        }
    }

    free(vdm);
    free(s_list);
    free(r_list);
    free(v_list);
    return (dist);
}

template u_int8_t* reed_sol_skipped_stream_decoding_matrix<64ul, 64ul>(int, int, int*, std::bitset<64ul>, std::bitset<64ul>, std::bitset<64ul>*);
template u_int8_t* reed_sol_skipped_stream_decoding_matrix<128ul, 128ul>(int, int, int*, std::bitset<128ul>, std::bitset<128ul>, std::bitset<128ul>*);

template <std::size_t N, std::size_t K> u_int8_t* reed_sol_skipped_stream_decoding_matrix(int n, int k,int* list, std::bitset<N> mask, std::bitset<N> arrive, std::bitset<K>* k_mask) {
    int i,j;
    uint8_t* vdm;
    uint8_t* dist;
    int rindex, tmp, rows, cols, mul_num, list_i, mask_i, kmask_j, now_s, now_r;
    int s_i,r_i,from_i,to_i;
    int* s_list;
    int* r_list;
    int* v_list;
    bool first_one;

    rows = n;
    cols = n*2;
    list_i=0;
    mask_i=0;
    now_r=0;
    now_s=0;

    // std::cout<<"Decode: n="<<n<<" k="<<k<<std::endl;
    // std::cout<<"Current Kmask:"<<std::endl;
    // for (int ii=0;ii<k;ii++) {
    //     std::cout<<k_mask[ii]<<std::endl;
    // }

    vdm = (u_int8_t*) malloc(sizeof(uint8_t) * rows * cols);
    dist = (u_int8_t*) malloc(sizeof(uint8_t) * k * n);
    s_list = (int*) malloc(sizeof(int)*n);
    r_list = (int*) malloc(sizeof(int)*k);
    v_list = (int*) malloc(sizeof(int)*k);
    if (vdm == NULL || dist == NULL || s_list == NULL || r_list == NULL || v_list == NULL){
        if (vdm!=NULL) free(vdm);
        if (dist!=NULL) free(dist);
        if (s_list!=NULL) free(s_list);
        if (r_list!=NULL) free(r_list);
        if (v_list!=NULL) free(v_list);
        return NULL;
    } 
    for (i=0;i<n;i++){
        while (mask_i < (int)N && mask[mask_i] == 0) {
            mask_i++;
        }
        if (mask_i >= (int)N) {
            if (vdm!=NULL) free(vdm);
            if (dist!=NULL) free(dist);
            if (s_list!=NULL) free(s_list);
            if (r_list!=NULL) free(r_list);
            if (v_list!=NULL) free(v_list);
            return NULL;
        }
        if (arrive[mask_i] == 0){
            if (list_i>=k) {
                if (vdm!=NULL) free(vdm);
                if (dist!=NULL) free(dist);
                if (s_list!=NULL) free(s_list);
                if (r_list!=NULL) free(r_list);
                if (v_list!=NULL) free(v_list);
                return NULL;
            }
            mul_num=list[list_i];
            tmp=1;
            rindex=i;
            first_one=false;
            kmask_j=0;
            for (j=0;j<n;j++){
                while (kmask_j < (int)N && mask[kmask_j] == 0) {
                    kmask_j++;
                    if (first_one) tmp = galois_single_multiply(tmp, mul_num, 8);
                }
                if (kmask_j >= (int)N) {
                    if (vdm!=NULL) free(vdm);
                    if (dist!=NULL) free(dist);
                    if (s_list!=NULL) free(s_list);
                    if (r_list!=NULL) free(r_list);
                    if (v_list!=NULL) free(v_list);
                    return NULL;
                }

                if (k_mask[list_i][kmask_j] == 1){
                    vdm[rindex*cols+j]=tmp;
                    if (first_one == false) first_one = true;
                }
                else {
                    vdm[rindex*cols+j]=0;
                }
                if (first_one) tmp = galois_single_multiply(tmp, mul_num, 8);
                kmask_j++;
            }
            r_list[now_r]=i;
            v_list[now_r]=mul_num;
            now_r++;
            list_i++;
        }
        else {
            for (j=0;j<n;j++){
                if (i==j) vdm[i*cols+j]=1;
                else vdm[i*cols+j]=0;
            }
            s_list[now_s]=i;
            now_s++;
        }
        mask_i++;
    }
    
    for (i=0;i<n;i++){
        for (j=0;j<n;j++){
            if (i==j) vdm[i*cols+n+j]=1;
            else vdm[i*cols+n+j]=0;
        }
    }

    // std::cout<<"baseNum list:"<<std::endl;
    // for (int ii=0;ii<k;ii++) {
    //     std::cout<<list[ii]<<" ";
    // }
    // std::cout<<std::endl;

    // std::cout<<"decoding matrix"<<std::endl;
    // for (int ii=0;ii<n;ii++) {
    //     for (int jj=0;jj<n*2;jj++) {
    //         std::cout<<(int)vdm[ii*n*2+jj]<<" ";
    //     }
    //     std::cout<<std::endl;
    // }

    // Get inverse matrix
    for (s_i=0;s_i<now_s;s_i++){
        from_i=s_list[s_i];
        for (r_i=0;r_i<now_r;r_i++){
            to_i=r_list[r_i];
            j=from_i;
            mul_num=vdm[to_i*cols+j];
            vdm[to_i*cols+j]=vdm[to_i*cols+j]^mul_num;
            vdm[to_i*cols+n+j]=vdm[to_i*cols+n+j]^mul_num;
        }
    }

    for (r_i=0;r_i<now_r;r_i++){
        from_i=r_list[r_i];
        if (vdm[from_i*cols+from_i]!=1){
            if (vdm[from_i*cols+from_i]==0){
                if (vdm!=NULL) free(vdm);
                if (dist!=NULL) free(dist);
                if (s_list!=NULL) free(s_list);
                if (r_list!=NULL) free(r_list);
                if (v_list!=NULL) free(v_list);
                return NULL;
            }
            tmp = galois_single_divide(1, vdm[from_i*cols+from_i], 8);
            for (j=0;j<cols;j++){
                vdm[from_i*cols+j]=galois_single_multiply(vdm[from_i*cols+j], tmp, 8);
            }
        }

        for (s_i=r_i+1;s_i<now_r;s_i++){
            to_i=r_list[s_i];
            mul_num=vdm[to_i*cols+from_i];
            for (j=0;j<cols;j++){
                tmp=galois_single_multiply(vdm[from_i*cols+j],mul_num, 8);
                vdm[to_i*cols+j]=vdm[to_i*cols+j]^tmp;
            }
        }
    }

    for (r_i=now_r-1;r_i>=0;r_i--){
        from_i=r_list[r_i];
        if (vdm[from_i*cols+from_i]!=1){
            tmp = galois_single_divide(1, vdm[from_i*cols+from_i], 8);
            for (j=0;j<cols;j++){
                vdm[from_i*cols+j]=galois_single_multiply(vdm[from_i*cols+j], tmp, 8);
            }
        }

        for (s_i=r_i-1;s_i>=0;s_i--){
            to_i=r_list[s_i];
            mul_num=vdm[to_i*cols+from_i];
            for (j=0;j<cols;j++){
                tmp=galois_single_multiply(vdm[from_i*cols+j],mul_num, 8);
                vdm[to_i*cols+j]=vdm[to_i*cols+j]^tmp;
            }
        }
    }

    for (i=0;i<now_r;i++){
        for (j=0;j<n;j++){
            dist[i*n+j]=vdm[r_list[i]*cols+n+j];
        }
    }

    // std::cout<<"inverse decoding matrix:"<<std::endl;
    // for (int ii=0;ii<n;ii++) {
    //     for (int jj=0;jj<n*2;jj++) {
    //         std::cout<<(int)vdm[ii*n*2+jj]<<" ";
    //     }
    //     std::cout<<std::endl;
    // }

    // std::cout<<"dist matrix:"<<std::endl;
    // for (int ii=0;ii<k;ii++) {
    //     for (int jj=0;jj<n;jj++) {
    //         std::cout<<(int)dist[ii*n+jj]<<" ";
    //     }
    //     std::cout<<std::endl;
    // }

    free(vdm);
    free(s_list);
    free(r_list);
    free(v_list);
    return (dist);
}