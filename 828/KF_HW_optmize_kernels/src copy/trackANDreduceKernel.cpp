/*

 Copyright (c) 2021 Computer Systems Lab - University of Thessaly

 This code is licensed under the MIT License.

*/

#define _HW_

// unoptimized HW implementation of the Tracking kernel

#include <kernels.hpp>
#include <string.h>


extern "C"{
void trackANDreduceKernel(float* reductionoutput, float4* inVertex,
		float4* inNormal, int inSize_x, int inSize_y, float4* refVertex,
		float4* refNormal, int refSize_x, int refSize_y, float* Ttrack_f,
		float* view_f, float dist_threshold,
		float normal_threshold) {

	#pragma HLS INTERFACE s_axilite port=return bundle=control

	#pragma HLS INTERFACE m_axi  port=reductionoutput offset=slave bundle=reductionoutput
	#pragma HLS INTERFACE s_axilite port=reductionoutput bundle=control

	#pragma HLS INTERFACE m_axi  port=inVertex offset=slave bundle=inVertex
	#pragma HLS INTERFACE s_axilite port=inVertex bundle=control

	#pragma HLS INTERFACE m_axi  port=inNormal offset=slave bundle=inNormal
	#pragma HLS INTERFACE s_axilite port=inNormal bundle=control

	#pragma HLS INTERFACE m_axi  port=refVertex offset=slave bundle=refVertex
	#pragma HLS INTERFACE s_axilite port=refVertex bundle=control

	#pragma HLS INTERFACE m_axi  port=refNormal offset=slave bundle=refNormal
	#pragma HLS INTERFACE s_axilite port=refNormal bundle=control

	#pragma HLS INTERFACE m_axi  port=Ttrack_f offset=slave bundle=Ttrack_f
	#pragma HLS INTERFACE s_axilite port=Ttrack_f bundle=control

	#pragma HLS INTERFACE m_axi  port=view_f offset=slave bundle=view_f
	#pragma HLS INTERFACE s_axilite port=view_f bundle=control

	#pragma HLS INTERFACE s_axilite port=inSize_x bundle=control
	#pragma HLS INTERFACE s_axilite port=inSize_y bundle=control
	#pragma HLS INTERFACE s_axilite port=refSize_x bundle=control
	#pragma HLS INTERFACE s_axilite port=refSize_y bundle=control
	#pragma HLS INTERFACE s_axilite port=dist_threshold bundle=control
	#pragma HLS INTERFACE s_axilite port=normal_threshold bundle=control

	#pragma HLS DATA_PACK variable=inVertex
	#pragma HLS DATA_PACK variable=inNormal
	#pragma HLS DATA_PACK variable=refVertex
	#pragma HLS DATA_PACK variable=refNormal



	Matrix4 Ttrack;
	Matrix4 view;
	int trackingResult;
	float trackingError;
	float trackingJ[6];
	#pragma HLS ARRAY_PARTITION variable=trackingJ type=complete
	float sums[8*32] = {0};
	#pragma HLS ARRAY_PARTITION variable=sums type=cyclic factor=8
	


	COPY_LOOP: for (int i = 0; i < 4; i ++) {
		#pragma HLS PIPELINE II=1
		Ttrack.data[i].x = Ttrack_f[i*4];
		Ttrack.data[i].y = Ttrack_f[i*4 + 1];
		Ttrack.data[i].z = Ttrack_f[i*4 + 2];
		Ttrack.data[i].w = Ttrack_f[i*4 + 3];
		view.data[i].x = view_f[i*4];
		view.data[i].y = view_f[i*4 + 1];
		view.data[i].z = view_f[i*4 + 2];
		view.data[i].w = view_f[i*4 + 3];
	}

	uint2 pixel = make_uint2(0, 0);
	unsigned int pixely, pixelx;

	for (pixely = 0; pixely < inSize_y; pixely++) {
        // in new_reduce, where the reductionoutput is processed with 8 blocks of size 32 
		int blockOffset = (pixely%8) * 32;

		#pragma HLS PIPELINE II=1
		for (pixelx = 0; pixelx < inSize_x; pixelx++) {
			#pragma HLS PIPELINE II=1
			pixel.x = pixelx;
			pixel.y = pixely;

			if (inNormal[pixel.x + pixel.y * inSize_x].x == KFUSION_INVALID) {
                trackingResult = -1;
				sums[blockOffset+29] += trackingResult == -4 ? 1 : 0;
				sums[blockOffset+30] += trackingResult == -5 ? 1 : 0;
				sums[blockOffset+31] += trackingResult > -4 ? 1 : 0;
				continue;
			}

			float3 projectedVertex = Ttrack * make_float3(inVertex[pixel.x + pixel.y * inSize_x]);
			float3 projectedPos = view * projectedVertex;
			float2 projPixel = make_float2(projectedPos.x / projectedPos.z + 0.5f, projectedPos.y / projectedPos.z + 0.5f);
			if (projPixel.x < 0 || projPixel.x > refSize_x - 1 ||
				projPixel.y < 0 || projPixel.y > refSize_y - 1) {
                trackingResult = -2;
				sums[blockOffset+29] += trackingResult == -4 ? 1 : 0;
				sums[blockOffset+30] += trackingResult == -5 ? 1 : 0;
				sums[blockOffset+31] += trackingResult > -4 ? 1 : 0;
				continue;
			}

			uint2 refPixel = make_uint2(projPixel.x, projPixel.y);
			float3 referenceNormal;
			referenceNormal.x = refNormal[refPixel.x + refPixel.y * refSize_x].x;
			referenceNormal.y = refNormal[refPixel.x + refPixel.y * refSize_x].y;
			referenceNormal.z = refNormal[refPixel.x + refPixel.y * refSize_x].z;

			if (referenceNormal.x == KFUSION_INVALID) {
                trackingResult = -3;
				sums[blockOffset+29] += trackingResult == -4 ? 1 : 0;
				sums[blockOffset+30] += trackingResult == -5 ? 1 : 0;
				sums[blockOffset+31] += trackingResult > -4 ? 1 : 0;
				continue;
			}

			float3 diff = make_float3(refVertex[refPixel.x + refPixel.y * refSize_x]) - projectedVertex;
			float3 projectedNormal = rotate(Ttrack, inNormal[pixel.x + pixel.y * inSize_x]);

			if (length(diff) > dist_threshold) {
                trackingResult = -4;
				sums[blockOffset+29] += trackingResult == -4 ? 1 : 0;
				sums[blockOffset+30] += trackingResult == -5 ? 1 : 0;
				sums[blockOffset+31] += trackingResult > -4 ? 1 : 0;
				continue;
			}
			if (dot(projectedNormal, referenceNormal) < normal_threshold) {
                trackingResult = -5;
				sums[blockOffset+29] += trackingResult == -4 ? 1 : 0;
				sums[blockOffset+30] += trackingResult == -5 ? 1 : 0;
				sums[blockOffset+31] += trackingResult > -4 ? 1 : 0;
				continue;
			}
            trackingResult = 1;

			trackingError = dot(referenceNormal, diff);

			float3 cross_res = cross(projectedVertex, referenceNormal);

			trackingJ[0] = referenceNormal.x;
			trackingJ[1] = referenceNormal.y;
			trackingJ[2] = referenceNormal.z;
			trackingJ[3] = cross_res.x;
			trackingJ[4] = cross_res.y;
			trackingJ[5] = cross_res.z;
            
			sums[blockOffset+0] += trackingError * trackingError;
			sums[blockOffset+1] += trackingError * trackingJ[0];
			sums[blockOffset+2] += trackingError * trackingJ[1];
			sums[blockOffset+3] += trackingError * trackingJ[2];
			sums[blockOffset+4] += trackingError * trackingJ[3];
			sums[blockOffset+5] += trackingError * trackingJ[4];
			sums[blockOffset+6] += trackingError * trackingJ[5];
			sums[blockOffset+7] += trackingJ[0] * trackingJ[0];
			sums[blockOffset+8] += trackingJ[0] * trackingJ[1];
			sums[blockOffset+9] += trackingJ[0] * trackingJ[2];
			sums[blockOffset+10] += trackingJ[0] * trackingJ[3];
			sums[blockOffset+11] += trackingJ[0] * trackingJ[4];
			sums[blockOffset+12] += trackingJ[0] * trackingJ[5];
			sums[blockOffset+13] += trackingJ[1] * trackingJ[1];
			sums[blockOffset+14] += trackingJ[1] * trackingJ[2];
			sums[blockOffset+15] += trackingJ[1] * trackingJ[3];
			sums[blockOffset+16] += trackingJ[1] * trackingJ[4];
			sums[blockOffset+17] += trackingJ[1] * trackingJ[5];
			sums[blockOffset+18] += trackingJ[2] * trackingJ[2];
			sums[blockOffset+19] += trackingJ[2] * trackingJ[3];
			sums[blockOffset+20] += trackingJ[2] * trackingJ[4];
			sums[blockOffset+21] += trackingJ[2] * trackingJ[5];
			sums[blockOffset+22] += trackingJ[3] * trackingJ[3];
			sums[blockOffset+23] += trackingJ[3] * trackingJ[4];
			sums[blockOffset+24] += trackingJ[3] * trackingJ[5];
			sums[blockOffset+25] += trackingJ[4] * trackingJ[4];
			sums[blockOffset+26] += trackingJ[4] * trackingJ[5];
            sums[blockOffset+27] += trackingJ[5] * trackingJ[5];
			sums[blockOffset+28] += 1;


		}
	}

	for(int i=0; i<8*32; i++){
	#pragma HLS PIPELINE II=1
		reductionoutput[i] = sums[i];
	}
}
}

