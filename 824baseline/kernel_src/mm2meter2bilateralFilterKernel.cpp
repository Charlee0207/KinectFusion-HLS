/*

 Copyright (c) 2021 Computer Systems Lab - University of Thessaly

 This code is licensed under the MIT License.

*/

#define _HW_

#include <kernels.hpp>
#include <string.h>


// unoptimized HW implementation of the Bilateral Filter kernel

extern "C" {


void mm2meter2bilateralFilterKernel(float* outDepth, 	
									ushort* inDepth, 
									uint inSize_x, uint inSize_y,
									uint outSize_x, uint outSize_y, 
									const float * gaussian, 
									float e_d, 
									int r) {


	#pragma HLS INTERFACE s_axilite port=return bundle=control
	#pragma HLS INTERFACE m_axi port=outDepth offset=slave bundle=outDepth max_read_burst_length=256 max_write_burst_length=256
	#pragma HLS INTERFACE s_axilite port=outDepth	bundle=control
	#pragma HLS INTERFACE m_axi port=inDepth offset=slave bundle=inDepth max_read_burst_length=256 max_write_burst_length=256
	#pragma HLS INTERFACE s_axilite port=inDepth	bundle=control
	#pragma HLS INTERFACE m_axi port=gaussian offset=slave bundle=gaussian
	#pragma HLS INTERFACE s_axilite port=gaussian	bundle=control
	#pragma HLS INTERFACE s_axilite port=inSize_x bundle=control
	#pragma HLS INTERFACE s_axilite port=inSize_y bundle=control
	#pragma HLS INTERFACE s_axilite port=outSize_x bundle=control
	#pragma HLS INTERFACE s_axilite port=outSize_y bundle=control
	#pragma HLS INTERFACE s_axilite port=e_d bundle=control
	#pragma HLS INTERFACE s_axilite port=r bundle=control


	uint y;
	float e_d_squared_2 = e_d * e_d * 2;
	int ratio = inSize_x / outSize_x;

	for (y = 0; y < inSize_y; y++) {
		#pragma HLS pipeline off
		for (uint x = 0; x < inSize_x; x++) {
			#pragma HLS pipeline off

			uint inPos = x * ratio + inSize_x * y * ratio;
			uint outPos = x + outSize_x * y;
			if (inDepth[inPos] == 0) {
				outDepth[outPos] = 0;
				continue;
			}

			float sum = 0.0f;
			float t = 0.0f;

			const float center = inDepth[inPos] / 1000.0f;

			for (int i = -r; i <= r; ++i) {
				#pragma HLS pipeline off
				for (int j = -r; j <= r; ++j) {
					#pragma HLS pipeline off
					uint2 outCurPos = make_uint2(clamp(x + i, 0u, outSize_x - 1), clamp(y + j, 0u, outSize_x - 1));
					uint2 inCurPos = make_uint2(outCurPos.x*ratio, outCurPos.y*ratio);
					const float curPix = inDepth[inCurPos.x + inCurPos.y * inSize_x] / 1000.0f;
					if (curPix > 0) {
						const float mod = sq(curPix - center);
						const float factor = gaussian[i + r] * gaussian[j + r] * expf(-mod / e_d_squared_2);
						t += factor * curPix;
						sum += factor;
					}
				}
			}
			outDepth[outPos] = t / sum;
		}
	}
}


}
