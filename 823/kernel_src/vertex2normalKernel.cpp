

#define _HW_

#include <kernels.hpp>
#include <string.h>


// unoptimized HW implementation of the vertex2normalKernel kernel

extern "C" {
void vertex2normalKernel(   float4* outNormal, 
                            float4* inVertex,
		                    uint inSize_x, uint inSize_y) {


	#pragma HLS INTERFACE s_axilite port=return bundle=control
	#pragma HLS INTERFACE m_axi port=outNormal offset=slave bundle=outNormal max_read_burst_length=256 max_write_burst_length=256
	#pragma HLS INTERFACE s_axilite port=outNormal	bundle=control

	#pragma HLS INTERFACE m_axi port=inVertex offset=slave bundle=inVertex max_read_burst_length=256 max_write_burst_length=256
	#pragma HLS INTERFACE s_axilite port=inVertex	bundle=control
	#pragma HLS INTERFACE s_axilite port=inSize_x bundle=control
	#pragma HLS INTERFACE s_axilite port=inSize_y bundle=control

    for (int y = 0; y < inSize_y; y++) {
    #pragma pipeline off
		for (int x = 0; x < inSize_x; x++) {
        #pragma pipeline off
			uint2 pleft = make_uint2(max(int(x) - 1, 0), y);
			uint2 pright = make_uint2(min(x + 1, (int) inSize_x - 1),
					y);
			uint2 pup = make_uint2(x, max(int(y) - 1, 0));
			uint2 pdown = make_uint2(x,
					min(y + 1, ((int) inSize_y) - 1));

			float4 left = inVertex[pleft.x + inSize_x * pleft.y];
			float4 right = inVertex[pright.x + inSize_x * pright.y];
			float4 up = inVertex[pup.x + inSize_x * pup.y];
			float4 down = inVertex[pdown.x + inSize_x * pdown.y];

			if (left.z == 0 || right.z == 0 || up.z == 0 || down.z == 0) {
				outNormal[x + y * inSize_x].x = KFUSION_INVALID;
				continue;
			}
		    float3 dxv = make_float3(right - left);
			float3 dyv = make_float3(down - up);
			outNormal[x + y * inSize_x] = make_float4(normalize(cross(dyv, dxv))); // switched dx and dy to get factor -1
		}
	}

}


}
