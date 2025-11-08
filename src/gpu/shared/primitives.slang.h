/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once

struct CircleParams {
    float radius;
    int _pad0[3];
};

struct Primitive {
	enum class Type: int {
		Circle,
	};
	Type type;
	int _pad0;
	float2 position;
    float2 velocity;
    int2 _pad1;
    float4 color;
#ifndef __cplusplus
    int params[4]; // reinterpret as one of the structs above
#else
	union {
		CircleParams circle_params;
	};
#endif
};
