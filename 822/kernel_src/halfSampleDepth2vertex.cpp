/*

 Copyright (c) 2021 Computer Systems Lab - University of Thessaly

 This code is licensed under the MIT License.

*/

#define _HW_

#include <kernels.hpp>
#include <string.h>


// unoptimized HW implementation of the Bilateral Filter kernel

extern "C" {


void mm2meter2bilateralFilterKernel(float4* outVertex0, float4* outVertex1, float4* outVertex2,
									ushort* inDepth, 
									uint inSize_x, uint inSize_y,
									float e_d, 
									int r, 
									Matrix4 invK) {


	#pragma HLS INTERFACE s_axilite port=return bundle=control
	#pragma HLS INTERFACE m_axi port=outVertex0 offset=slave bundle=outVertex0 max_read_burst_length=256 max_write_burst_length=256
	#pragma HLS INTERFACE s_axilite port=outVertex0	bundle=control
	#pragma HLS INTERFACE m_axi port=outVertex1 offset=slave bundle=outVertex1 max_read_burst_length=256 max_write_burst_length=256
	#pragma HLS INTERFACE s_axilite port=outVertex1	bundle=control
	#pragma HLS INTERFACE m_axi port=outVertex2 offset=slave bundle=outVertex2 max_read_burst_length=256 max_write_burst_length=256
	#pragma HLS INTERFACE s_axilite port=outVertex2	bundle=control

	#pragma HLS INTERFACE m_axi port=inDepth offset=slave bundle=inDepth max_read_burst_length=256 max_write_burst_length=256
	#pragma HLS INTERFACE s_axilite port=inDepth	bundle=control
	#pragma HLS INTERFACE s_axilite port=inSize_x bundle=control
	#pragma HLS INTERFACE s_axilite port=inSize_y bundle=control
	#pragma HLS INTERFACE s_axilite port=e_d bundle=control
	#pragma HLS INTERFACE s_axilite port=r bundle=control

	uint2 outSizeLevel0 = make_uint2(inSize_x, inSize_y);
	uint2 outSizeLevel1 = make_uint2(inSize_x/2, inSize_y/2);
	uint2 outSizeLevel2 = make_uint2(inSize_x/4, inSize_y/4);
	uint2 block4x4Num = make_uint2(inSize_x/4, inSize_y/4);

	float blockLevel0[4][4];
	float blockLevel1[2][2];
	float blockLevel2;
	
	
	for(int block_y=0; block_y<block4x4Num.y; block_y++){
		for(int block_x=0; block_x<block4x4Num.x; block_x++){
			uint x_offset = block_x * 4;
			uint y_offset = block_y * block4x4Num.x * 16;
			// Read a 4x4 block depth and compute vertex
			for(int i=0; i<4; i++){
				for(int j=0; j<4; j++){
					blockLevel0[i][j] = inDepth[(block_x_offset + block_y_offset) + (j + inSize_x*i)];
				}
			}


			float sum0Lv1=0.0f, sum1Lv1=0.0f, sum2Lv1=0.0f, sum3Lv1=0.0f;
			float t0Lv1=0.0f, t1Lv1=0.0f, t2Lv1=0.0f, t3Lv1=0.0f;
			float centerLv1;
			// Half sample and compute vertex
			for(int i=0; i<2; i++){
				for(int j=0; j<2; j++){
					centerLv1 = blockLevel0[i*2][j*2];
					if(fabsf(blockLevel0[i*2][j*2] - centerLv1) < e_d){sum0Lv1 = 1.0f; t0Lv1 = blockLevel0[i*2][j*2];}
					if(fabsf(blockLevel0[i*2][j*2+1] - centerLv1) < e_d){sum1Lv1 = 1.0f; t1Lv1 = blockLevel0[i*2][j*2+1];}
					if(fabsf(blockLevel0[i*2+1][j*2] - centerLv1) < e_d){sum2Lv1 = 1.0f; t2Lv1 = blockLevel0[i*2+1][j*2];}
					if(fabsf(blockLevel0[i*2+1][j*2+1] - centerLv1) < e_d){sum3Lv1 = 1.0f; t3Lv1 = blockLevel0[i*2+1][j*2+1];}

					blockLevel1[i][j] = (t0Lv1+t1Lv1+t2Lv1+t3Lv1) / (sum0Lv1+sum1Lv1+sum2Lv1+sum3Lv1);

				}
			}

			// Quarter sample and compute vertex
			float sum0Lv2=0.0f, sum1Lv2=0.0f, sum2Lv2=0.0f, sum3Lv2=0.0f;
			float t0Lv2=0.0f, t1Lv2=0.0f, t2Lv2=0.0f, t3Lv2=0.0f;
			float centerLv2 = blockLevel1[0][0];
			if(fabsf(blockLevel1[0][0] - centerLv2) < e_d){sum0Lv2 = 1.0f; t0Lv2 = blockLevel1[0][0];}
			if(fabsf(blockLevel1[0][1] - centerLv2) < e_d){sum1Lv2 = 1.0f; t1Lv2 = blockLevel1[0][1];}
			if(fabsf(blockLevel1[1][0] - centerLv2) < e_d){sum2Lv2 = 1.0f; t2Lv2 = blockLevel1[1][0];}
			if(fabsf(blockLevel1[1][1] - centerLv2) < e_d){sum3Lv2 = 1.0f; t3Lv2 = blockLevel1[1][1];}

			blockLevel2 = (t0Lv2+t1Lv2+t2Lv2+t3Lv2) / (sum0Lv2+sum1Lv2+sum2Lv2+sum3Lv2);
		}
	}

	for (y = 0; y < outSize.y; y++) {
		for (unsigned int x = 0; x < outSize.x; x++) {
			uint2 pixel = make_uint2(x, y);
			const uint2 centerPixel = 2 * pixel;

			float sum = 0.0f;
			float t = 0.0f;
			const float center = in[centerPixel.x + centerPixel.y * inSize.x];
			for (int i = 0; i <= 1; ++i) {
				for (int j = 0; j <= 1; ++j) {
					uint2 cur = make_uint2(
							clamp( 	make_int2(centerPixel.x + j, centerPixel.y + i), make_int2(0),
									make_int2(2 * outSize.x - 1, 2 * outSize.y - 1)));
					float current = in[cur.x + cur.y * inSize.x];
					if (fabsf(current - center) < e_d) {
						sum += 1.0f;
						t += current;
					}
				}
			}
			out[pixel.x + pixel.y * outSize.x] = t / sum;
		}
	}
}


}
