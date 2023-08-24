/*

 Copyright (c) 2021 Computer Systems Lab - University of Thessaly

 This code is licensed under the MIT License.

*/

#define _HW_

#include <kernels.hpp>
#include <string.h>

#define WIDTH 320
#define HEIGHT 240
#define WINDOW_SIZE 5
#define E_DELTA 0.1f
#define RADIUS 2

#define E_DELTA_SQUR_2 = RADIUS * RADIUS * 2;

float gaussian[WINDOW_SIZE][WINDOW_SIZE] = \
{0.778801, 0.855345, 0.882497, 0.855345, 0.778801, 
0.855345, 0.939413, 0.969233, 0.939413, 0.855345,
0.882497, 0.969233, 1       , 0.969233, 0.882497,
0.855345, 0.939413, 0.969233, 0.939413, 0.855345,
0.778801, 0.855345, 0.882497, 0.855345, 0.778801};

/*
void generateGaussian(){
	float gaussian_tmp[5];
    float gaussian[5][5];
    
	for (unsigned int i = 0; i < 5; i++) {
		int x = i - 2;
		gaussian_tmp[i] = expf(-(x * x) / (2 * 4.0f * 4.0f));
    }
    for(int i=0; i<5; i++){
        for(int j=0; j<5; j++){
            gaussian[i][j] = gaussian_tmp[i] * gaussian_tmp[j];
		}
    }
}
*/	

extern "C" {


void bilateralFilterKernel(float* out, float* in, uint size_x, uint size_y,
		const float * gaussian, float e_d, int r) {


	#pragma HLS INTERFACE s_axilite port=return bundle=control
	#pragma HLS INTERFACE m_axi port=out offset=slave bundle=out max_read_burst_length=256 max_write_burst_length=256
	#pragma HLS INTERFACE s_axilite port=out	bundle=control
	#pragma HLS INTERFACE m_axi port=in offset=slave bundle=pad_depth max_read_burst_length=256 max_write_burst_length=256
	#pragma HLS INTERFACE s_axilite port=in	bundle=control
	#pragma HLS INTERFACE m_axi port=gaussian offset=slave bundle=gaussian
	#pragma HLS INTERFACE s_axilite port=gaussian	bundle=control
	#pragma HLS INTERFACE s_axilite port=size_x bundle=control
	#pragma HLS INTERFACE s_axilite port=size_y bundle=control

	float lineBuffer0[WIDTH+RADIUS*2] = {0};
	#pragma HLS ARRAY_PARTITION variable=lineBuffer0 cyclic factor=5
	float lineBuffer1[WIDTH+RADIUS*2] = {0};
	#pragma HLS ARRAY_PARTITION variable=lineBuffer1 cyclic factor=5
	float lineBuffer2[WIDTH+RADIUS*2] = {0};
	#pragma HLS ARRAY_PARTITION variable=lineBuffer2 cyclic factor=5
	float lineBuffer3[WIDTH+RADIUS*2] = {0};
	#pragma HLS ARRAY_PARTITION variable=lineBuffer3 cyclic factor=5
	float lineBuffer4[WIDTH+RADIUS*2] = {0};
	#pragma HLS ARRAY_PARTITION variable=lineBuffer4 cyclic factor=5

	float mod[WINDOW_SIZE][WINDOW_SIZE];
	#pragma HLS ARRAY_PARTITION variable=mod complete
	float factor[WINDOW_SIZE][WINDOW_SIZE];
	#pragma HLS ARRAY_PARTITION variable=factor complete


	// Preload the upper 2 rows into line buffer
	for(int y=0; y<RADIUS; y++){
		for(int x=0; x<WIDTH+RADIUS*2; x++){
			lineBuffer3[x] = lineBuffer4[x];
			lineBuffer4[x] = (x>=RADIUS&&x<WIDTH+RADIUS)? in[x+y*WIDTH] \	
														: ((x<RADIUS) ? in[y*WIDTH] : in[(y+1)*WIDTH-1]);
			// Preload the default value for others
			lineBuffer0[x] = lineBuffer4[x];
			lineBuffer1[x] = lineBuffer4[x];
			lineBuffer2[x] = lineBuffer4[x];
		}
	}


	for (int y=0; y<HEIGHT; y++) {
		for (int x=0; x<WIDTH+RADIUS*2; x++) {

			int pos = x + y * WIDTH;

			// Shift the line buffer
			lineBuffer0[lx] = lineBuffer1[lx];
			lineBuffer1[lx] = lineBuffer2[lx];
			lineBuffer2[lx] = lineBuffer3[lx];
			lineBuffer3[lx] = lineBuffer4[lx];
			lineBuffer4[lx] = (ly>=HEIGHT) ? lineBuffer4[lx] : in[pos + RADIUS*WIDTH];
			

			float sum = 0.0f;
			float t = 0.0f;
			const float center = lineBuffer2[lx]; //in[pos]

			// i=-2
			for(int j=0; j<WINDOW_SIZE; j++){
				mod[0][j] = sq(lineBuffer0[x+j] - center);
				mod[1][j] = sq(lineBuffer1[x+j] - center);
				mod[2][j] = sq(lineBuffer2[x+j] - center);
				mod[3][j] = sq(lineBuffer3[x+j] - center);
				mod[4][j] = sq(lineBuffer4[x+j] - center);
				factor[0][j] = gaussian[0][j] * expf(-mod/E_DELTA_SQUR_2) ;
				factor[1][j] = gaussian[0][j] * expf(-mod/E_DELTA_SQUR_2) ;
				factor[2][j] = gaussian[0][j] * expf(-mod/E_DELTA_SQUR_2) ;
				factor[3][j] = gaussian[0][j] * expf(-mod/E_DELTA_SQUR_2) ;
				factor[4][j] = gaussian[0][j] * expf(-mod/E_DELTA_SQUR_2) ;
			}
			// for (int i = -r; i <= r; ++i) {
			// 	#pragma HLS pipeline off
			// 	for (int j = -r; j <= r; ++j) {
			// 		#pragma HLS pipeline off
					// uint2 curPos = make_uint2(clamp(x + i, 0u, size_x - 1), clamp(y + j, 0u, size_y - 1));
					// const float curPix = in[curPos.x + curPos.y * size_x];
					if (curPix > 0) {
						const float mod = sq(curPix - center);
						const float factor = gaussian[i + r] * gaussian[j + r] * expf(-mod / e_d_sqrd);
						t += factor * curPix;
						sum += factor;
					}
				}
			}
			out[pos] = t / sum;
		}
	}
}


}
