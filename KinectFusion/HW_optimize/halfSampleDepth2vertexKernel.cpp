

#define _HW_

#include <kernels.hpp>
#include <string.h>


extern "C" {


void halfSampleDepth2vertexKernel(	float4* outVertex0, float4* outVertex1, float4* outVertex2,
									float* inDepth, 
									uint inSize_x, uint inSize_y,
									float e_d, 
									int r, 
									float* K) {


	#pragma HLS INTERFACE s_axilite port=return bundle=control
	#pragma HLS INTERFACE m_axi port=outVertex0 offset=slave bundle=outVertex0 max_read_burst_length=256 max_write_burst_length=256
	#pragma HLS INTERFACE s_axilite port=outVertex0	bundle=control
	#pragma HLS INTERFACE m_axi port=outVertex1 offset=slave bundle=outVertex1 max_read_burst_length=256 max_write_burst_length=256
	#pragma HLS INTERFACE s_axilite port=outVertex1	bundle=control
	#pragma HLS INTERFACE m_axi port=outVertex2 offset=slave bundle=outVertex2 max_read_burst_length=256 max_write_burst_length=256
	#pragma HLS INTERFACE s_axilite port=outVertex2	bundle=control
	#pragma HLS INTERFACE m_axi port=K offset=slave bundle=K max_read_burst_length=256 max_write_burst_length=256
	#pragma HLS INTERFACE s_axilite port=K bundle=control

	#pragma HLS INTERFACE m_axi port=inDepth offset=slave bundle=inDepth max_read_burst_length=256 max_write_burst_length=256
	#pragma HLS INTERFACE s_axilite port=inDepth  bundle=control
	#pragma HLS INTERFACE s_axilite port=inSize_x bundle=control
	#pragma HLS INTERFACE s_axilite port=inSize_y bundle=control
	#pragma HLS INTERFACE s_axilite port=e_d bundle=control
	#pragma HLS INTERFACE s_axilite port=r bundle=control


	uint2 outSizeLevel0 = make_uint2(inSize_x, inSize_y);
	uint2 outSizeLevel1 = make_uint2(inSize_x/2, inSize_y/2);
	uint2 outSizeLevel2 = make_uint2(inSize_x/4, inSize_y/4);
	// uint2 block4x4Num = make_uint2(inSize_x/4, inSize_y/4);
	#define BLOCK4x4NUM_X 80 
	#define BLOCK4x4NUM_Y 60


	float depthBlockLevel0[4][4];
	#pragma HLS ARRAY_PARTITION variable=depthBlockLevel0 dim=1 complete
	float depthBlockLevel1[2][2];
	#pragma HLS ARRAY_PARTITION variable=depthBlockLevel1 dim=1 complete
	float depthBlockLevel2;

	float4 vertexBlockLevel0[4][4];
	#pragma HLS ARRAY_PARTITION variable=vertexBlockLevel0 dim=1 complete
	float4 vertexBlockLevel1[2][2];
	#pragma HLS ARRAY_PARTITION variable=vertexBlockLevel1 dim=1 complete
	
	float4 vertexBlockLevel2;
	float4 cameraK = make_float4(K[0], K[1], K[2], K[3]);
	Matrix4 invK0 = getInverseCameraMatrix(cameraK / float(1 << 0));
	Matrix4 invK1 = getInverseCameraMatrix(cameraK / float(1 << 1));
	Matrix4 invK2 = getInverseCameraMatrix(cameraK / float(1 << 2));
	
	
	for(int block_y=0; block_y<BLOCK4x4NUM_Y; block_y++){
	#pragma HLS PIPELINE II=1
		for(int block_x=0; block_x<BLOCK4x4NUM_X; block_x++){
		#pragma HLS PIPELINE II=1
			uint x_offset = block_x * 4;
			uint y_offset = block_y * BLOCK4x4NUM_X * 16;
			// Read a 4x4 block depth and compute vertex
			for(int i=0; i<4; i++){
				#pragma HLS PIPELINE II=1
				// Copy the data in burst: max_read_burst_length=256
				memcpy(depthBlockLevel0[i], inDepth+(x_offset + y_offset)+(inSize_x*i), sizeof(float)*4);
				for(int j=0; j<4; j++){
					vertexBlockLevel0[i][j] = (depthBlockLevel0[i][j] > 0) \
					? make_float4(depthBlockLevel0[i][j] * (rotate(invK0, make_float3(block_x*4+j, block_y*4+i, 1.f)))) \
					: make_float4(0);							
				}
			}
			
			// Half sample and compute vertex
			float sum0Lv1, sum1Lv1, sum2Lv1, sum3Lv1;
			float t0Lv1, t1Lv1, t2Lv1, t3Lv1;
			bool diff_e_dComp0Lv1, diff_e_dComp1Lv1, diff_e_dComp2Lv1, diff_e_dComp3Lv1;
			float centerLv1;

			// Half sample and compute vertex
			float sum0Lv1=0.0f, sum1Lv1=0.0f, sum2Lv1=0.0f, sum3Lv1=0.0f;
			float t0Lv1=0.0f, t1Lv1=0.0f, t2Lv1=0.0f, t3Lv1=0.0f;
			float centerLv1;
			// Unroll the block
			for(int i=0; i<2; i++){
			#pragma HLS PIPELINE II=1
				for(int j=0; j<2; j++){
				#pragma HLS UNROLL
					centerLv1 = depthBlockLevel0[i*2][j*2];
					if(fabsf(depthBlockLevel0[i*2][j*2] - centerLv1) < e_d){sum0Lv1 = 1.0f; t0Lv1 = depthBlockLevel0[i*2][j*2];}
					if(fabsf(depthBlockLevel0[i*2][j*2+1] - centerLv1) < e_d){sum1Lv1 = 1.0f; t1Lv1 = depthBlockLevel0[i*2][j*2+1];}
					if(fabsf(depthBlockLevel0[i*2+1][j*2] - centerLv1) < e_d){sum2Lv1 = 1.0f; t2Lv1 = depthBlockLevel0[i*2+1][j*2];}
					if(fabsf(depthBlockLevel0[i*2+1][j*2+1] - centerLv1) < e_d){sum3Lv1 = 1.0f; t3Lv1 = depthBlockLevel0[i*2+1][j*2+1];}

					depthBlockLevel1[i][j] = (t0Lv1+t1Lv1+t2Lv1+t3Lv1) / (sum0Lv1+sum1Lv1+sum2Lv1+sum3Lv1);
					vertexBlockLevel1[i][j] = (depthBlockLevel1[i][j] > 0) \
					? make_float4(depthBlockLevel1[i][j] * (rotate(invK1, make_float3(block_x*2+j, block_y*2+i, 1.f)))) \
					: make_float4(0);
				}
			}


			// Quarter sample and compute vertex
			float sum0Lv2=0.0f, sum1Lv2=0.0f, sum2Lv2=0.0f, sum3Lv2=0.0f;
			float t0Lv2=0.0f, t1Lv2=0.0f, t2Lv2=0.0f, t3Lv2=0.0f;
			float centerLv2 = depthBlockLevel1[0][0];
			if(fabsf(depthBlockLevel1[0][0] - centerLv2) < e_d){sum0Lv2 = 1.0f; t0Lv2 = depthBlockLevel1[0][0];}
			if(fabsf(depthBlockLevel1[0][1] - centerLv2) < e_d){sum1Lv2 = 1.0f; t1Lv2 = depthBlockLevel1[0][1];}
			if(fabsf(depthBlockLevel1[1][0] - centerLv2) < e_d){sum2Lv2 = 1.0f; t2Lv2 = depthBlockLevel1[1][0];}
			if(fabsf(depthBlockLevel1[1][1] - centerLv2) < e_d){sum3Lv2 = 1.0f; t3Lv2 = depthBlockLevel1[1][1];}

			depthBlockLevel2 = (t0Lv2+t1Lv2+t2Lv2+t3Lv2) / (sum0Lv2+sum1Lv2+sum2Lv2+sum3Lv2);
			vertexBlockLevel2 = (depthBlockLevel2 > 0) \
			? make_float4(depthBlockLevel2 * (rotate(invK2, make_float3(block_x, block_y, 1.f)))) \
			: make_float4(0);

			// Write back the vertex result
			for(int i=0; i<4; i++){
				memcpy(outVertex0+(x_offset + y_offset)+(outSizeLevel0.x*i), vertexBlockLevel0[i], sizeof(float4)*4);
			}
			for(int i=0; i<2; i++){
				memcpy(outVertex1+(x_offset/2 + y_offset/4)+(outSizeLevel0.x*i), vertexBlockLevel1[i], sizeof(float4)*2);
			}
			outVertex2[x_offset/4 + y_offset/16] = vertexBlockLevel2;					
		}
	}
}


}
