#include "obj_file.h"


static unsigned long long get_file_size(HANDLE file){
	unsigned long long n;
	GetFileSizeEx(file,(union _LARGE_INTEGER*)&n);
	return n;
}


static char **load_file_as_lines(HANDLE file,int *_num_lines){
	unsigned long long file_size=get_file_size(file);

	char *buf=(char*)malloc(file_size);
	unsigned long bytes_read;
	assert(ReadFile(file,buf,file_size,&bytes_read,0));
	assert(bytes_read==file_size);

	int num_lines=0;
    for(int i=0;i<file_size;i++){
    	if(buf[i]=='\n'){ 
	        buf[i]=0;
	        num_lines++;
	    }	
    }
    
    char **lines=(char**)malloc(sizeof(char*)*num_lines);
    lines[0]=buf;
    if(num_lines>1){
        int j=1;
        char p=1;
        for(int i=0;i<file_size;i++){
            if(buf[i]==0 && p!=0){
                lines[j]=&buf[i+1];
                j++;
                if(j>=num_lines) break;
            }
        }
    }
    if(_num_lines) *_num_lines=num_lines;
    return lines;
}


static wchar_t *a2w_strncpy(wchar_t *dest,const char *src,size_t n){
	size_t i;
	for(i=0;i<n && src[i]!=0;i++) dest[i]=src[i];
	for(;i<n;i++) dest[i]=L'\0';
	return dest;
}

static wchar_t *dirname_w(const wchar_t *path){
    static wchar_t dir[MAX_PATH];
    const wchar_t *last_backslash=NULL;

    int len=wcslen(path);
    for(int i=0;i<len;i++){
    	if(path[i]==L'\\' || path[i]==L'/') last_backslash=&path[i];
    }

    if(last_backslash==NULL){
        wcscpy(dir, L".\\");
    }else{
        wcsncpy(dir,path,last_backslash-path);
        dir[last_backslash-path+0]=L'\\';
        dir[last_backslash-path+1]=L'\0';
    }

    return dir;
}



static bmp_t load_bmp(const wchar_t *file_name){
   __pragma(pack(push,1)) struct bmp_header{
	    uint16_t magic; //0x4d42
	    uint32_t file_size;
	    uint32_t app; //0
	    uint32_t offset; //54
	    uint32_t info_size; //40
	    int32_t width;
	    int32_t height;
	    uint16_t planes; //1
	    uint16_t bits_per_pix; //32 (four bytes)
	    uint32_t comp; //0
	    uint32_t comp_size; //size in bytes of the pixel buffer (w*h*4)
	    uint32_t xres; //0
	    uint32_t yres; //0
	    uint32_t cols_used; //0
	    uint32_t imp_cols; //0
	}__pragma(pack(pop,1));

	static_assert(sizeof(bmp_header)==54,"");



	HANDLE file=CreateFileW(file_name,GENERIC_READ,0,0,OPEN_EXISTING,0,0);
	WIN32_CHECK(file!=INVALID_HANDLE_VALUE);

	unsigned long long file_size=get_file_size(file);
    
    unsigned long m;
	bmp_header head={0};
	assert(ReadFile(file,&head,sizeof head,&m,0));
	assert(m==sizeof head);

    assert(head.magic==0x4d42);
    assert(head.app==0);
    assert(head.offset==sizeof(bmp_header));
    assert(head.info_size==40);
    assert(head.planes==1);
    // assert(head.bits_per_pix==32);
    // assert(head.comp_size==head.width*head.height*head.bits_per_pix/8);
    // assert(head.file_size==sizeof(bmp_header)+head.comp_size);


    bmp_t bmp={0};
    bmp.w=head.width;
    bmp.h=head.height;
    bmp.bpp=head.bits_per_pix;

    const int size=bmp.bpp*bmp.h*bmp.w/8;

    bmp.pixels=malloc(size);
    // bmp.pixels=malloc(head.comp_size);

    assert(ReadFile(file,bmp.pixels,size,&m,0));
	assert(m==size);

	assert(bmp.bpp==32 || bmp.bpp==24);
	const int inc=bmp.bpp/8;

	auto ptr=(uint8_t*)bmp.pixels;
    for(int i=0;i<bmp.w*bmp.h;i++){
    	uint8_t tmp=ptr[inc*i+0];
    	ptr[inc*i+0]=ptr[inc*i+2];
    	ptr[inc*i+2]=tmp;
    }

    CloseHandle(file);
    // println("done");
    return bmp;
}

static uint32_t make_texture(void *pixels,int w,int h,int bpp){
	uint32_t tex_id;
    int fmt;
    switch(bpp){
    case 32: fmt=GL_RGBA;break;
    case 24: fmt=GL_RGB;break;
    default: assert(0);
    }
	glGenTextures(1, &tex_id);
    assert(tex_id>0);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, fmt, GL_UNSIGNED_BYTE,pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return tex_id;
}

void create_obj_file(obj_file *of,const wchar_t *fname){
	fprintf(stderr,"%ws\n",fname);



	memset(of,0,sizeof(obj_file));

	HANDLE file=CreateFileW(fname,GENERIC_READ,0,0,OPEN_EXISTING,0,0);
	WIN32_CHECK(file!=INVALID_HANDLE_VALUE);

	int num_lines=0;
	char **lines=load_file_as_lines(file,&num_lines);
	CloseHandle(file);

    std::vector<float3> verts;
    std::vector<float3> norms;
    std::vector<float2> txcds;
    std::vector<of_face_elem_t> felms;
    std::vector<of_mtl_t> metls;
  	felms.reserve(326786);
  	assert(felms.size()==0);

    int metl_idx=0;
    
    int prev_elems_in_poly=0;
    for(int i=0;i<num_lines;i++){
    	char mtl2use[64];
    	if(lines[i][0]=='\0') continue;
    	if(lines[i][0]=='#') continue;
    	else if(1==sscanf(lines[i],"usemtl %s\r",mtl2use)){
    		bool matched=0;
    		lprintln(mtl2use);
    		for(int k=0;k<metls.size();k++){
    			lprintln(metls[k].name);
    			if(0==strncmp(metls[k].name,mtl2use,sizeof mtl2use)){
    				metl_idx=k+1;
    				matched=1;
    			}
    		}
    		assert(matched);
    	}else if(0==memcmp(lines[i],"vn",2)){
			float3 v;
			int matched=sscanf(lines[i],"vn %f %f %f\n",&v.x,&v.y,&v.z);
			assert(matched==3);
			norms.push_back(v);
    	}else if(0==memcmp(lines[i],"vt",2)){
			float2 v;
			int matched=sscanf(lines[i],"vt %f %f\n",&v.x,&v.y);
			assert(matched==2);
			txcds.push_back(v);
    	}else if(lines[i][0]=='v'){
			float3 v;
			int matched=sscanf(lines[i],"v %f %f %f\n",&v.x,&v.y,&v.z);
			assert(matched==3);
			verts.push_back(v);
    	}else if(lines[i][0]=='f'){
    		const char *itr=lines[i]+1;
    		int elems_in_poly=0;
    		while(1){
    			const auto consume_int=[](const char *itr)->const char*{
					char *endptr=NULL;
    				strtol(itr,&endptr,10);
    				assert(endptr);
    				assert(endptr>itr);
    				return endptr;
    			};

	    		of_face_elem_t felm={0};
	    		if(0){
	    		}else if(3==sscanf(itr," %d/%d/%d",&felm.vert_idx,&felm.txcd_idx,&felm.norm_idx)){
	    			itr=consume_int(itr+1);
	    			itr=consume_int(itr+1);
	    			itr=consume_int(itr+1);
	    		}else if(2==sscanf(itr," %d/%d",   &felm.vert_idx,&felm.txcd_idx)){
	    			itr=consume_int(itr+1);
	    			itr=consume_int(itr+1);
	    		}else if(2==sscanf(itr," %d//%d",  &felm.vert_idx,&felm.norm_idx)){
	    			itr=consume_int(itr+1);
	    			itr=consume_int(itr+2);
	    		}else if(1==sscanf(itr," %d",      &felm.vert_idx)){
	    			itr=consume_int(itr+1);
	    		}else{
	    			assert(elems_in_poly>=3);
	    			felms[felms.size()-elems_in_poly].vert_cnt=elems_in_poly;
	    			break;
	    		}
	    		felm.metl_idx=metl_idx;
				felms.push_back(felm);
	    		elems_in_poly++;
    		}
    		prev_elems_in_poly=elems_in_poly;
    	}else if(0==memcmp(lines[i],"mtllib ",7)){

    		char *mtl_name=lines[i]+7;
    		int n=strlen(mtl_name);
    		if(mtl_name[n-1]=='\r') mtl_name[n-1]=0;
    		lprintln(mtl_name);

    		wchar_t *dir=dirname_w(fname);
    		a2w_strncpy(dir+wcslen(dir),mtl_name,MAX_PATH);
    		lprintln(dir);

    		HANDLE file=CreateFileW(dir,GENERIC_READ,0,0,OPEN_EXISTING,0,0);
			WIN32_CHECK(file!=INVALID_HANDLE_VALUE);
    		
    		int num_mtl_lines=0;
			char **mtl_lines=load_file_as_lines(file,&num_mtl_lines);
			CloseHandle(file);

			bool push_mtl=0;
			of_mtl_t mtl={0};
			for(int k=0;k<num_mtl_lines;k++){
				if(mtl_lines[k][0]==0) continue;
				if(mtl_lines[k][0]=='#') continue;

				float3 v;
				float f;
				int illum;
				char img_name[MAX_PATH];
				char newmtl_name[64];

				if(0){
				}else if(1==sscanf(mtl_lines[k],"newmtl %s\r",newmtl_name)){
					if(push_mtl) metls.push_back(mtl);
					else push_mtl=1;
					memset(&mtl,0,sizeof mtl);
					strncpy(mtl.name,newmtl_name,sizeof newmtl_name);
				}else if(3==sscanf(mtl_lines[k],"\tKa %f %f %f",&v.x,&v.y,&v.z)){
					mtl.ka=v;
				}else if(3==sscanf(mtl_lines[k],"\tKd %f %f %f",&v.x,&v.y,&v.z)){
					mtl.kd=v;
				}else if(3==sscanf(mtl_lines[k],"\tKs %f %f %f",&v.x,&v.y,&v.z)){
					mtl.ks=v;
				}else if(3==sscanf(mtl_lines[k],"\tTf %f %f %f",&v.x,&v.y,&v.z)){
					mtl.tf=v;
				}else if(1==sscanf(mtl_lines[k],"\tNs %f",&f)){
					mtl.ns=f;
				}else if(1==sscanf(mtl_lines[k],"\tTr %f",&f)){
					mtl.tr=f;
				}else if(1==sscanf(mtl_lines[k],"\tNi %f",&f)){
					mtl.ni=f;
				}else if(1==sscanf(mtl_lines[k],"\tillum %d",&illum)){
					mtl.illum=illum;
				}else if(1==sscanf(mtl_lines[k],"\tmap_Ka %s\r",img_name)){

				}else if(1==sscanf(mtl_lines[k],"\tmap_Kd %s\r",img_name)){
					// lprintln(img_name);
					wchar_t *dir=dirname_w(fname);
		    		a2w_strncpy(dir+wcslen(dir),img_name,MAX_PATH);
		    		// lprintln(dir);
					bmp_t bmp=load_bmp(dir);
					mtl.map_kd.bmp=bmp;
					mtl.map_kd.id=make_texture(bmp.pixels,bmp.w,bmp.h,bmp.bpp);
				}else if(1==sscanf(mtl_lines[k],"\tmap_Ks %s\r",img_name)){

				}

			}
			if(push_mtl) metls.push_back(mtl);

			free(mtl_lines[0]);
			free(mtl_lines);
		
		}
    }

	if(norms.size()==0){
		int inc=3;
		for(size_t i=0;i<felms.size();i+=inc){
			of_face_elem_t f=felms[i];
			assert(f.vert_cnt);
			inc=f.vert_cnt;
			float3 v0=verts[felms[i+0].vert_idx-1];
			float3 v1=verts[felms[i+1].vert_idx-1];
			float3 v2=verts[felms[i+2].vert_idx-1];
			float3 n=glm::normalize(glm::cross(v1-v0,v2-v0));
			norms.push_back(n);
			for(int j=0;j<inc;j++){
				felms[i+j].norm_idx=norms.size();
			}
		}
	
	}

    of->verts=(float3*)malloc(sizeof(float3)*verts.size());
	of->norms=(float3*)malloc(sizeof(float3)*norms.size());
	of->txcds=(float2*)malloc(sizeof(float2)*txcds.size());
	of->felms=(of_face_elem_t*)malloc(sizeof(of_face_elem_t)*felms.size());
	of->metls=(of_mtl_t*)malloc(sizeof(of_mtl_t)*metls.size());

	memcpy(of->verts,&verts[0],verts.size()*sizeof(verts[0]));
	memcpy(of->norms,&norms[0],norms.size()*sizeof(norms[0]));
	memcpy(of->txcds,&txcds[0],txcds.size()*sizeof(txcds[0]));	
	memcpy(of->felms,&felms[0],felms.size()*sizeof(felms[0]));
	memcpy(of->metls,&metls[0],metls.size()*sizeof(metls[0]));	

	of->num_verts=verts.size();
	of->num_norms=norms.size();
	of->num_txcds=txcds.size();
	of->num_felms=felms.size();
	of->num_metls=metls.size();


	of->vert_cols=(float3*)malloc(sizeof(float3)*verts.size());
	of->vert_norms=(float3*)malloc(sizeof(float3)*verts.size());
	of->vert_txcds=(float2*)malloc(sizeof(float3)*verts.size());

	float2x3 box(float3(1e20),float3(-1e20));
	for(int i=0;i<of->num_verts;i++){
		box[0]=glm::min(box[0],of->verts[i]);
		box[1]=glm::max(box[1],of->verts[i]);
	}
	of->box=box;

    for(size_t i=0;i<of->num_felms;i++){
        const of_face_elem_t f=of->felms[i];
		const float3 v=of->verts[f.vert_idx-1];
        
        
        const float3 n=of->norms[f.norm_idx-1];
        of->vert_norms[f.vert_idx-1]=n;

		float2 uv;
		if(f.txcd_idx==0){
			uv.x=map(v.x,box[0].x,box[1].x,0,1);
			uv.y=map(v.y,box[0].y,box[1].y,0,1);
		}else{
			uv=of->txcds[f.txcd_idx-1];
		}
		of->vert_txcds[f.vert_idx-1]=uv;

		if(f.metl_idx==0){
			of->vert_cols[f.vert_idx-1]=(n+1.0f)/2.0f;
		}else{
			const bmp_t bmp=of->metls[f.metl_idx-1].map_kd.bmp;
			const int2 ij=uv*float2(bmp.w,bmp.h);
			const uint8_t *pix=(uint8_t*)bmp.pixels;
			const int off=(ij.x+bmp.w*ij.y)*(bmp.bpp/8);
			const float3 c=float3(pix[off+0],pix[off+1],pix[off+2])/255.0f;
			of->vert_cols[f.vert_idx-1]=c;
		}
    }

	lprintln(of->num_verts);
	lprintln(of->num_txcds);
	lprintln(of->num_norms);
	lprintln(of->num_felms);


	free(lines[0]);
	free(lines);
}

void destroy_obj_file(obj_file *of){
	if(of->verts) free(of->verts);
	if(of->norms) free(of->norms);
	if(of->txcds) free(of->txcds);
	if(of->felms) free(of->felms);
	if(of->vert_norms) free(of->vert_norms);
	if(of->vert_txcds) free(of->vert_txcds);
	if(of->vert_cols) free(of->vert_cols);
	if(of->metls){
		for(int i=0;i<of->num_metls;i++){
			uint32_t tex_id=of->metls[i].map_kd.id;
			if(tex_id) glDeleteTextures(1,&tex_id);
			void *p=of->metls[i].map_kd.bmp.pixels;
			if(p) free(p);
		}
		free(of->metls);
	}
	memset(of,0,sizeof(obj_file));
}

void glTexCoord(float2 v){
    glTexCoord2fv((float*)&v);
}

void draw_obj_file(obj_file *of,geo_mode_e gm,col_mode_e cm){
	float s=0;
	for(int i=0;i<3;i++){
		s=glm::max(s,glm::abs(of->box[1][i]-of->box[0][i]));
	}
	glPushMatrix();
	glRotatef(-90,1,0,0);
	glRotatef(90,0,0,1);
	glTranslate(-(of->box[0]+of->box[1])/2.0f/s);
	glScalef(1/s,1/s,1/s);
	glColor3f(0,1,1);

	if(gm==GM_VERTS){
		size_t n=of->num_verts;
		glBegin(GL_POINTS);
		for(size_t i=0;i<n;i++){

			switch(cm){
			case CM_NORMS: glColor((1.0f+of->vert_norms[i])/2.0f);break;
    		case CM_TXCDS: glColor3f(of->vert_txcds[i].x,0,of->vert_txcds[i].y);break;
    		case CM_TEXTS: glColor(of->vert_cols[i]);break;
			}
			glVertex<3>(of->verts[i]);
		}
		glEnd();
		glPopMatrix();
		return;
	}

	size_t n=of->num_felms;
	if(gm==GM_EDGES){
		int inc=3;
		for(size_t i=0;i<n;i+=inc){
			of_face_elem_t f1=of->felms[i];
			inc=f1.vert_cnt;
			glBegin(GL_LINE_STRIP);
			for(int j=0;j<inc+1;j++){
				of_face_elem_t f2=of->felms[i+j%inc];
				float2 txcd;
				if(f2.txcd_idx==0) txcd=of->vert_txcds[f2.vert_idx-1];
				else txcd=of->txcds[f2.txcd_idx-1];
				switch(cm){
				case CM_NORMS: glColor((1.0f+of->norms[f2.norm_idx-1])/2.0f);break;
	    		case CM_TXCDS: glColor3f(txcd.x,0,txcd.y);break;
	    		case CM_TEXTS: glColor(of->vert_cols[f2.vert_idx-1]);break;
				}
				glVertex<3>(of->verts[f2.vert_idx-1]);
			}
			glEnd();
		}
		glPopMatrix();
		return;
	}

	assert(gm==GM_FACES);

	if(cm==CM_TEXTS && of->num_metls==0) cm=CM_NORMS;


	glColor3f(1,1,1);
	if(cm==CM_TEXTS) glEnable(GL_TEXTURE_2D);

	int prev_metl_idx=-1;
	int prev_vert_cnt=3;
	glBegin(GL_TRIANGLES);
	for(size_t i=0;i<n;i++){
		of_face_elem_t f=of->felms[i];

		const bool c1=(f.vert_cnt!=0 && f.vert_cnt!=prev_vert_cnt);
		const bool c2=(f.metl_idx!=0 && f.metl_idx!=prev_metl_idx);
		if(c1 || c2){
			glEnd();

			if(c2){
				uint32_t tex_id=of->metls[f.metl_idx-1].map_kd.id;
				glBindTexture(GL_TEXTURE_2D,tex_id);
				prev_metl_idx=f.metl_idx;
			}

			switch(f.vert_cnt){
				case 3: glBegin(GL_TRIANGLES);break;
				case 4: glBegin(GL_QUADS);break;
				default:
					glBegin(GL_TRIANGLES);
			}
			prev_vert_cnt=f.vert_cnt;
		}

		float2 txcd;
		if(f.txcd_idx==0) txcd=of->vert_txcds[f.vert_idx-1];
		else txcd=of->txcds[f.txcd_idx-1];

		switch(cm){
			case CM_NORMS: glColor((1.0f+of->norms[f.norm_idx-1])/2.0f);break;
			case CM_TXCDS: glColor3f(txcd.x,0,txcd.y);break;
			case CM_TEXTS: glTexCoord(txcd);break;
			default:assert(0);
		}
		glVertex<3>(of->verts[f.vert_idx-1]);
	}
	glEnd();

	if(cm==CM_TEXTS) glDisable(GL_TEXTURE_2D);
	glPopMatrix();
}


// void save_obj_file(obj_file *of,const wchar_t *fname){
// 	FILE *file=_wfopen(fname,L"wb");
// 	const size_t num_verts=of->num_verts;
//     const size_t num_norms=of->num_norms;
//     const size_t num_txcds=of->num_txcds;
//     const size_t num_faces=of->num_faces;

// 	for(int i=0;i<num_verts;i++){
// 		float3 v=of->verts[i];
// 		fprintf(file,"v %f %f %f\n",v.x,v.y,v.z);
// 	}
// 	for(int i=0;i<num_norms;i++){
// 		float3 v=of->norms[i];
// 		fprintf(file,"vn %f %f %f\n",v.x,v.y,v.z);
// 	}
// 	for(int i=0;i<num_txcds;i++){
// 		float2 v=of->txcds[i];
// 		fprintf(file,"vt %f %f\n",v.x,v.y);
// 	}
// 	for(int i=0;i<num_faces;i++){
// 		of_face face=of->faces[i];
// 		fprintf(file,"f");
// 		for(int j=0;j<3;j++){
// 			fprintf(file," %d",face.a[j].vert_idx);
// 			if(face.a[j].txcd_idx==0 && face.a[j].norm_idx==0) continue;
// 			fprintf(file,"/");
// 			if(face.a[j].txcd_idx) fprintf(file,"%d",face.a[j].txcd_idx);
// 			if(face.a[j].norm_idx) fprintf(file,"/%d",face.a[j].norm_idx);
// 		}
// 		fprintf(file,"\n");
// 	}

// 	fclose(file);
// }