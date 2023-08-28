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

float gaussian[WINDOW_SIZE][WINDOW_SIZE] ={
{0.778801, 0.855345, 0.882497, 0.855345, 0.778801}, 
{0.855345, 0.939413, 0.969233, 0.939413, 0.855345},
{0.882497, 0.969233, 1       , 0.969233, 0.882497},
{0.855345, 0.939413, 0.969233, 0.939413, 0.855345},
{0.778801, 0.855345, 0.882497, 0.855345, 0.778801}};

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
		const float * gaussian_disable, float e_d, int r) {


	#pragma HLS INTERFACE s_axilite port=return bundle=control
	#pragma HLS INTERFACE m_axi port=out offset=slave bundle=out max_read_burst_length=256 max_write_burst_length=256
	#pragma HLS INTERFACE s_axilite port=out	bundle=control
	#pragma HLS INTERFACE m_axi port=in offset=slave bundle=pad_depth max_read_burst_length=256 max_write_burst_length=256
	#pragma HLS INTERFACE s_axilite port=in	bundle=control
	#pragma HLS INTERFACE m_axi port=gaussian_disable offset=slave bundle=gaussian_disable
	#pragma HLS INTERFACE s_axilite port=gaussian_disable	bundle=control
	#pragma HLS INTERFACE s_axilite port=size_x bundle=control
	#pragma HLS INTERFACE s_axilite port=size_y bundle=control
	#pragma HLS INTERFACE s_axilite port=e_d bundle=control
	#pragma HLS INTERFACE s_axilite port=r bundle=control


	float lineBuffer0[WIDTH] = {0};
	#pragma HLS ARRAY_PARTITION variable=lineBuffer0 cyclic factor=5
	float lineBuffer1[WIDTH] = {0};
	#pragma HLS ARRAY_PARTITION variable=lineBuffer1 cyclic factor=5
	float lineBuffer2[WIDTH] = {0};
	#pragma HLS ARRAY_PARTITION variable=lineBuffer2 cyclic factor=5
	float lineBuffer3[WIDTH] = {0};
	#pragma HLS ARRAY_PARTITION variable=lineBuffer3 cyclic factor=5
	float lineBuffer4[WIDTH] = {0};
	#pragma HLS ARRAY_PARTITION variable=lineBuffer4 cyclic factor=5

	float windowBuffer[WINDOW_SIZE][WINDOW_SIZE];
	#pragma HLS ARRAY_PARTITION variable=windowBuffer complete
	float mod[WINDOW_SIZE][WINDOW_SIZE];
	#pragma HLS ARRAY_PARTITION variable=mod complete
	float factor[WINDOW_SIZE][WINDOW_SIZE];
	#pragma HLS ARRAY_PARTITION variable=factor complete

	bool colBoundaryCondition[5];
	float e_delta_squr_2 = (RADIUS * RADIUS * 2);

	// Initialize the line buffer and window buffer
	for(int y=0; y<RADIUS; y++){
		for(int x=0; x<WIDTH; x++){
		#pragma pipeline II=1
			lineBuffer0[x] = lineBuffer1[x];
			lineBuffer1[x] = lineBuffer2[x];
			lineBuffer2[x] = lineBuffer3[x];
			lineBuffer3[x] = lineBuffer4[x];
			lineBuffer4[x] = in[x+y*WIDTH];
		}
	}
	for(int x=0; x<RADIUS; x++){
	#pragma pipeline II=1
		windowBuffer[0][3] = windowBuffer[0][4];
		windowBuffer[1][3] = windowBuffer[1][4];
		windowBuffer[2][3] = windowBuffer[2][4];
		windowBuffer[3][3] = windowBuffer[3][4];
		windowBuffer[4][3] = windowBuffer[4][4];
		// Shift the line buffer
		windowBuffer[0][4] = (lineBuffer0[x] = lineBuffer1[x]);
		windowBuffer[1][4] = (lineBuffer1[x] = lineBuffer2[x]);
		windowBuffer[2][4] = (lineBuffer2[x] = lineBuffer3[x]);
		windowBuffer[3][4] = (lineBuffer3[x] = lineBuffer4[x]);
		windowBuffer[4][4] = (lineBuffer4[x] = in[x + RADIUS*WIDTH]);
	}


	for (int y=0; y<HEIGHT; y++) {
	#pragma HLS PIPELINE II=1
		for(int x=0; x<WIDTH; x++) {
		#pragma HLS PIPELINE II=1

			int pos = x + y * WIDTH;

			// Fill the window buffer
			for(int j=0; j<WINDOW_SIZE-1; j++){
			#pragma PIPELINE II=1
				windowBuffer[0][j] = windowBuffer[0][j+1];
				windowBuffer[1][j] = windowBuffer[1][j+1];
				windowBuffer[2][j] = windowBuffer[2][j+1];
				windowBuffer[3][j] = windowBuffer[3][j+1];
				windowBuffer[4][j] = windowBuffer[4][j+1];
			}

			// Shift the line buffer
			windowBuffer[0][4] = (lineBuffer0[x] = lineBuffer1[x]);
			windowBuffer[1][4] = (lineBuffer1[x] = lineBuffer2[x]);
			windowBuffer[2][4] = (lineBuffer2[x] = lineBuffer3[x]);
			windowBuffer[3][4] = (lineBuffer3[x] = lineBuffer4[x]);
			windowBuffer[4][4] = (lineBuffer4[x] = (y>=HEIGHT)	? lineBuffer4[x]
																: in[pos + RADIUS*WIDTH]);
			
			float sum = 0.0f;
			float t = 0.0f;
			const float center = windowBuffer[2][2]; //in[pos]

			colBoundaryCondition[0] = (x<RADIUS);
			colBoundaryCondition[1] = (x<RADIUS-1);
			colBoundaryCondition[2] = true;
			colBoundaryCondition[3] = (x>WIDTH-RADIUS);
			colBoundaryCondition[4] = (x>WIDTH-RADIUS-1);

			
			for(int j=0; j<WINDOW_SIZE; j++){
				#pragma HLS UNROLL
				mod[0][j] = sq(windowBuffer[(y<RADIUS)?2:0			][(colBoundaryCondition[j])?2:j] - center);
				mod[1][j] = sq(windowBuffer[(y<RADIUS-1)?2:1		][(colBoundaryCondition[j])?2:j] - center);
				mod[2][j] = sq(windowBuffer[2						][(colBoundaryCondition[j])?2:j] - center);
				mod[3][j] = sq(windowBuffer[(y>HEIGHT-RADIUS)?2:3	][(colBoundaryCondition[j])?2:j] - center);
				mod[4][j] = sq(windowBuffer[(y>HEIGHT-RADIUS-1)?2:4	][(colBoundaryCondition[j])?2:j] - center);
				factor[0][j] = gaussian[0][j] * expf(-mod[0][j]/e_delta_squr_2) ;
				factor[1][j] = gaussian[1][j] * expf(-mod[1][j]/e_delta_squr_2) ;
				factor[2][j] = gaussian[2][j] * expf(-mod[2][j]/e_delta_squr_2) ;
				factor[3][j] = gaussian[3][j] * expf(-mod[3][j]/e_delta_squr_2) ;
				factor[4][j] = gaussian[4][j] * expf(-mod[4][j]/e_delta_squr_2) ;
				t = factor[0][j]*windowBuffer[(y<RADIUS)?2:0			][(colBoundaryCondition[j])?2:j]
				  + factor[1][j]*windowBuffer[(y<RADIUS-1)?2:1			][(colBoundaryCondition[j])?2:j]
				  + factor[2][j]*windowBuffer[2							][(colBoundaryCondition[j])?2:j]
				  + factor[3][j]*windowBuffer[(y>HEIGHT-RADIUS)?2:3		][(colBoundaryCondition[j])?2:j]
				  + factor[4][j]*windowBuffer[(y>HEIGHT-RADIUS-1)?2:4	][(colBoundaryCondition[j])?2:j];
				sum = factor[0][j] + factor[1][j] + factor[2][j] + factor[3][j] + factor[4][j];
			}

			out[pos] = t / sum;
		}
	}
}
}