#pragma once
#include "common.h"

struct _quat{
    float s;
    float3 v;
	// _quat():s(1),v(float3(0,0,0)){}
    // constexpr _quat(float _s,float3 _v):s(_s),v(_v){}
};

_quat quat_conj(_quat q);
_quat operator*(const _quat l,const _quat r);
_quat &operator*=(_quat &l,const _quat r);
bool operator==(const _quat l,const _quat r);
float get_angle(_quat q);
float3x3 to_mtx3(_quat q);
float4x4 to_mtx(_quat q);
float3 rotate(float3 v,_quat q);
_quat angle_axis(float theta,float3 n);
_quat from_to(float3 a,float3 b);
void glRotate(_quat);
template<> int print<_quat>(_quat q){return print(q.s,'+',q.v);}