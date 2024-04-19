#include "las_file.h"

static_assert(sizeof(char)==1,"");
static_assert(sizeof(unsigned char)==1,"");
static_assert(sizeof(short)==2,"");
static_assert(sizeof(unsigned short)==2,"");
static_assert(sizeof(long)==4,"");
static_assert(sizeof(unsigned long)==4,"");
static_assert(sizeof(long long)==8,"");
static_assert(sizeof(unsigned long long)==8,"");
static_assert(sizeof(float)==4,"");
static_assert(sizeof(double)==8,"");




typedef __pragma(pack(push,1)) struct{
	char file_signature[4];
	unsigned short file_source_id;
	unsigned short global_encoding;
	unsigned long  project_id_guid_data_1;
	unsigned short project_id_guid_data_2;
	unsigned short project_id_guid_data_3;
	unsigned char  project_id_guid_data_4[8];
	unsigned char  version_major;
	unsigned char  version_minor;
	char system_identifier[32];
	char generating_software[32];
	unsigned short file_creation_day_of_year;
	unsigned short file_creation_year;
	unsigned short header_size;
	unsigned long  offset_to_point_data;
	unsigned long  number_of_variable_length_records;
	unsigned char  point_data_record_format;
	unsigned short point_data_record_length;
	unsigned long  legacy_number_of_point_records;
	unsigned long  legacy_number_of_points_by_return[5];
	double x_scale_factor;
	double y_scale_factor;
	double z_scale_factor;
	double x_offset;
	double y_offset;
	double z_offset;
	double max_x;
	double min_x;
	double max_y;
	double min_y;
	double max_z;
	double min_z;
	unsigned long long start_of_waveform_data_packet_record;
	unsigned long long start_of_first_extended_variable_length_record;
	unsigned long      number_of_extended_variable_length_records;
	unsigned long long number_of_point_records;
	unsigned long long number_of_points_by_return[15];
}__pragma(pack(pop,1)) las_header;

static_assert(sizeof(las_header)==375,"");


typedef __pragma(pack(push,1)) struct{
	unsigned short reserved;
	char user_id[16];
	unsigned short record_id;
	unsigned short record_length_after_header;
	char description[32];
	char data[];
}__pragma(pack(pop,1)) vlr_header;

static_assert(sizeof(vlr_header)==54,"");


typedef __pragma(pack(push,1)) struct{
	long x;  
	long y;  
	long z;  
	unsigned short intensity;
	char return_number:4;
	char number_of_returns:4;
	char classification_flags:4;
	char scanner_channel:2;
	char scan_direction_flag:1;
	char edge_of_flight_line:1;
	unsigned char classification;
	unsigned char user_data;
	short scan_angle;
	unsigned short point_source_id;
	double gps_time;
	unsigned short red;
	unsigned short green;
	unsigned short blue;
}__pragma(pack(pop,1)) point_data_record_format7;

static_assert(sizeof(point_data_record_format7)==36,"");

static unsigned long long get_file_size(HANDLE file){
	unsigned long long n;
	GetFileSizeEx(file,(union _LARGE_INTEGER*)&n);
	return n;
}


void save_las_file(las_file *lf,const wchar_t *fname){
	FILE *file=_wfopen(fname,L"wb");
	las_header header={0};
	header.file_signature[0]='L';
	header.file_signature[1]='A';
	header.file_signature[2]='S';
	header.file_signature[3]='F';
	header.global_encoding=4;
	header.version_major=1;
	header.version_minor=4;
	header.header_size=sizeof(las_header);
	header.offset_to_point_data=sizeof(las_header);
	header.point_data_record_format=7;
	header.point_data_record_length=sizeof(point_data_record_format7);
	header.number_of_point_records=lf->num_points;

	size_t num_points=lf->num_points;

	// for(size_t i=0;i<num_points;i++){
	// 	lf->points[i].v=lf->points[i].v*lf->scale_factor;
	// }



	float2x3 box(float3(1e20),float3(-1e20));
	for(size_t i=0;i<num_points;i++){
		box[0]=glm::min(box[0],lf->points[i].v);
		box[1]=glm::max(box[1],lf->points[i].v);
	}



	header.min_x=box[0].x;
	header.min_y=box[0].y;
	header.min_z=box[0].z;
	header.max_x=box[1].x;
	header.max_y=box[1].y;
	header.max_z=box[1].z;

	const float sf=1e4;

	header.x_scale_factor=(1/sf);
	header.y_scale_factor=(1/sf);
	header.z_scale_factor=(1/sf);

	fwrite(&header,1,sizeof(header),file);

	for(size_t i=0;i<num_points;i++){
		point_data_record_format7 rec={0};
		vert_data vd=lf->points[i];
		rec.x=vd.v.x/header.x_scale_factor;
		rec.y=vd.v.y/header.y_scale_factor;
		rec.z=vd.v.z/header.z_scale_factor;
		rec.red   =vd.col.x*65536;
		rec.green =vd.col.y*65536;
		rec.blue  =vd.col.z*65536;
		rec.classification=vd._class;

		fwrite(&rec,1,sizeof(point_data_record_format7),file);
	}
	fclose(file);
}

void create_las_file(las_file *lf,const wchar_t *fname){
	wprintf(L"%ws\n",fname);
	HANDLE file=CreateFileW(fname,GENERIC_READ,0,0,OPEN_EXISTING,0,0);
	assert(file!=INVALID_HANDLE_VALUE);


	unsigned long long file_size=get_file_size(file);
	// PRINT(file_size,"%llu");

	char *buf=(char*)malloc(file_size);
	unsigned long bytes_read;
	assert(ReadFile(file,buf,file_size,&bytes_read,0)); //like read()
	assert(bytes_read==file_size);

	auto header=(las_header*)buf;
	assert(header->file_signature[0]=='L');
	assert(header->file_signature[1]=='A');
	assert(header->file_signature[2]=='S');
	assert(header->file_signature[3]=='F');
	assert(header->version_major==1);
	assert(header->version_minor==4);
	// PRINT(header->system_identifier,"%s");
	// PRINT(header->generating_software,"%s");
	assert(header->header_size==375);
	// PRINT(header->offset_to_point_data,"%lu");
	// PRINT(header->number_of_variable_length_records,"%lu");
	// PRINT(header->point_data_record_format,"%hhu");
	assert(header->point_data_record_format<=10);
	assert(header->point_data_record_format==7);
	// PRINT(header->point_data_record_length,"%hu");

	// PRINT(header->legacy_number_of_point_records,"%lu");
	// PRINT(header->legacy_number_of_points_by_return,"%lu");

	// printf("scale :[%f,%f,%f]\n",header->x_scale_factor,header->y_scale_factor,header->z_scale_factor);
	// printf("offset:[%f,%f,%f]\n",header->x_offset,header->y_offset,header->z_offset);
	// printf("min   :[%f,%f,%f]\n",header->min_x,header->min_y,header->min_z);
	// printf("max   :[%f,%f,%f]\n",header->max_x,header->max_y,header->max_z);

	assert(header->min_x<=header->max_x);
	assert(header->min_y<=header->max_y);
	assert(header->min_z<=header->max_z);


	// PRINT(header->start_of_waveform_data_packet_record,"%llu");
	// PRINT(header->start_of_first_extended_variable_length_record,"%llu");
	// PRINT(header->number_of_extended_variable_length_records,"%lu");
	// PRINT(header->number_of_point_records,"%llu");

	auto vlrh=(vlr_header*)(buf+sizeof(las_header));
	// PRINT(vlrh->user_id,"%s");
	// PRINT(vlrh->record_id,"%hu");
	// PRINT(vlrh->record_length_after_header,"%hu");
	// PRINT(vlrh->description,"%s");

	// assert(header->offset_to_point_data==
	// 	sizeof(las_header)+sizeof(vlr_header)+vlrh->record_length_after_header);

	assert(file_size==header->offset_to_point_data+
		header->number_of_point_records*header->point_data_record_length);


	auto vbuf=(vert_data*)malloc(header->number_of_point_records*sizeof(vert_data));

	lf->num_points=header->number_of_point_records;
	lf->points=vbuf;


	double delta=.01;
	header->min_x-=delta;
	header->min_y-=delta;
	header->min_z-=delta;
	header->max_x+=delta;
	header->max_y+=delta;
	header->max_z+=delta;
	

	float bcube_max=glm::max(header->max_x,glm::max(header->max_y,header->max_z));
	float bcube_min=glm::min(header->min_x,glm::min(header->min_y,header->min_z));



	float3 scale_factor=float3(header->x_scale_factor,header->y_scale_factor,header->z_scale_factor);
	float3 bbmax=float3(header->max_x,header->max_y,header->max_z);
	float3 bbmin=float3(header->min_x,header->min_y,header->min_z);
	float3 bbsize=bbmax-bbmin;
	// float bbsize_max=300;//glm::max(glm::max(bbsize.x,bbsize.y),bbsize.z);

	// lf->scale_factor=bbsize_max;
	lf->box[0]=bbmin;
	lf->box[1]=bbmax;
	// lf->avg=float3(0);
	
	
	char *data_stt=buf+header->offset_to_point_data;
	char *data_end=buf+file_size;
	char *ptr;
	unsigned long long point_size=header->point_data_record_length;
	unsigned long long num_points=header->number_of_point_records;
	unsigned long long i;
	for(i=0,ptr=data_stt; i<num_points && ptr<data_end; i++,ptr+=point_size){
		auto pdr=(point_data_record_format7*)ptr;
		
		float3 p=float3(pdr->x,pdr->y,pdr->z);
		p*=scale_factor;
		// p/=bbsize_max;
		vbuf[i].v=p;
		float3 col;
		col.r=pdr->red/65536.0;
		col.g=pdr->green/65536.0;
		col.b=pdr->blue/65536.0;

		vbuf[i].col=col;
		vbuf[i]._class=pdr->classification;
		// assert(vbuf[i]._class<pc_count);
		// assert(vbuf[i]._class!=pc_ulcer);
		
		// lf->avg+=p;
	}
	// lf->avg.z/=num_points;
	// lf->avg=float3(0);

	free(buf);
	CloseHandle(file);
}
