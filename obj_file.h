#pragma once
#include "common.h"


struct bmp_t{
    int w;
    int h;
    int bpp;
    void *pixels;
};


struct texture_t{
    uint32_t id;
    bmp_t bmp;   
};


struct of_mtl_t{
    char name[64];
    uint8_t illum;
    float3 ka; //ambient
    float3 kd; //diffuse
    float3 ks; //specular
    float ns;  //specular exponent
    float tr;  //transparency
    float3 tf; //transparency filter
    float ni;  //index of refraction
    texture_t map_ka; 
    texture_t map_kd; 
    texture_t map_ks; 
};

struct of_face_elem_t{
    int txcd_idx;
    int norm_idx;
    int vert_idx;
    uint8_t metl_idx;
    uint8_t vert_cnt;
};

struct obj_file{
	size_t num_verts;
    size_t num_norms;
    size_t num_txcds;
    float3 *verts;
    float3 *norms;
    float2 *txcds;
    float3 *vert_cols;
    float3 *vert_norms;
    float2 *vert_txcds;
    size_t num_felms;
    of_face_elem_t *felms;
    float2x3 box;
    size_t num_metls;
    of_mtl_t *metls;
}; 

enum geo_mode_e{
    GM_VERTS,
    GM_EDGES,
    GM_FACES,
};

enum col_mode_e{
    CM_NORMS,
    CM_TXCDS,
    CM_TEXTS,
};

void create_obj_file(obj_file*,const wchar_t*);
void save_obj_file(obj_file*,const wchar_t*);
void destroy_obj_file(obj_file*);
void draw_obj_file(obj_file*,geo_mode_e,col_mode_e);