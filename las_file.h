#pragma once
#include "common.h"

typedef struct{
	float3 v;
	float3 col;
    unsigned char _class;
}vert_data;

struct las_file{
    float2x3 box;
	size_t num_points;
	vert_data *points;
}; 

void create_las_file(las_file*,const wchar_t*);
void save_las_file(las_file*,const wchar_t*);