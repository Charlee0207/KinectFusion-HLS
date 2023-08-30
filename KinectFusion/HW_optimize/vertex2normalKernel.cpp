


#define _HW_

#include <kernels.hpp>
#include <string.h>


#define EPSILON 1e-6
#define WIDTH 320
#define HEIGHT 240
#define WINDOW_SIZE 3
#define RADIUS 1


// unoptimized HW implementation of the vertex2normalKernel kernel

extern "C" {
void vertex2normalKernel(   float4* outNormal, 
                            float4* inVertex,
		                    uint inSize_x, uint inSize_y) {


	#pragma HLS INTERFACE s_axilite port=return bundle=control
	#pragma HLS INTERFACE m_axi port=outNormal offset=slave bundle=outNormal 
	#pragma HLS INTERFACE s_axilite port=outNormal	bundle=control

	#pragma HLS INTERFACE m_axi port=inVertex offset=slave bundle=inVertex 
	#pragma HLS INTERFACE s_axilite port=inVertex bundle=control
	#pragma HLS INTERFACE s_axilite port=inSize_x bundle=control
	#pragma HLS INTERFACE s_axilite port=inSize_y bundle=control

	float4 lineBuffer0[WIDTH] = {0};
	#pragma HLS ARRAY_PARTITION variable=lineBuffer0 cyclic factor=5
	float4 lineBuffer1[WIDTH] = {0};
	#pragma HLS ARRAY_PARTITION variable=lineBuffer1 cyclic factor=5
	float4 lineBuffer2[WIDTH] = {0};
	#pragma HLS ARRAY_PARTITION variable=lineBuffer2 cyclic factor=5

	float4 windowBuffer[WINDOW_SIZE][WINDOW_SIZE];
	#pragma HLS ARRAY_PARTITION variable=windowBuffer complete


	// Initialize the line buffer and window buffer
	for(int y=0; y<RADIUS; y++){
	#pragma HLS PIPELINE II=1
		for(int x=0; x<inSize_x; x++){
		#pragma HLS LOOP_TRIPCOUNT min=80 max=320
		#pragma HLS PIPELINE II=1
			lineBuffer0[x] = lineBuffer1[x];
			lineBuffer1[x] = lineBuffer2[x];
			lineBuffer2[x] = inVertex[x+y*WIDTH];
		}
	}
	for(int x=0; x<RADIUS; x++){
	#pragma HLS PIPELINE II=1
		windowBuffer[0][1] = windowBuffer[0][2];
		windowBuffer[1][1] = windowBuffer[1][2];
		windowBuffer[2][1] = windowBuffer[2][2];
		// Shift the line buffer
		windowBuffer[0][2] = (lineBuffer0[x] = lineBuffer1[x]);
		windowBuffer[1][2] = (lineBuffer1[x] = lineBuffer2[x]);
		windowBuffer[2][2] = (lineBuffer2[x] = inVertex[x + RADIUS*WIDTH]);
	}

    for (int y = 0; y < inSize_y; y++) {
	#pragma HLS LOOP_TRIPCOUNT min=60 max=240
	#pragma HLS PIPELINE II=1
		for (int x = 0; x < inSize_x; x++) {
		#pragma HLS LOOP_TRIPCOUNT min=80 max=320
		#pragma HLS PIPELINE II=1

			int pos = x + y * inSize_x;

			// Fill the window buffer
			for(int j=0; j<WINDOW_SIZE-1; j++){
			#pragma HLS PIPELINE II=1
				windowBuffer[0][j] = windowBuffer[0][j+1];
				windowBuffer[1][j] = windowBuffer[1][j+1];
				windowBuffer[2][j] = windowBuffer[2][j+1];
			}

			// Shift the line buffer
			windowBuffer[0][2] = (lineBuffer0[x] = lineBuffer1[x]);
			windowBuffer[1][2] = (lineBuffer1[x] = lineBuffer2[x]);
			windowBuffer[2][2] = (lineBuffer2[x] = (y>=inSize_y)? lineBuffer2[x]
																: inVertex[pos + RADIUS*inSize_x]);
			
			float4 left 	= windowBuffer[1						][(x<RADIUS)?1:0];
			float4 right 	= windowBuffer[1						][(x>inSize_x-RADIUS-1)?1:2];
			float4 up 		= windowBuffer[(y<RADIUS)?1:0			][1];
			float4 down 	= windowBuffer[(y>inSize_y-RADIUS-1)?1:2][1];

			if (fabsf(left.z) < EPSILON || fabsf(right.z) < EPSILON  || fabsf(up.z) < EPSILON  || fabsf(down.z) < EPSILON ) {
				outNormal[x + y * inSize_x] = make_float4(KFUSION_INVALID, 0.f, 0.f, 0.f);
			}
			else{
				float3 dxv = make_float3(right - left);
				float3 dyv = make_float3(down - up);
				outNormal[x + y * inSize_x] = make_float4(normalize(cross(dyv, dxv))); // switched dx and dy to get factor -1
			}
		}
	}

}


}
