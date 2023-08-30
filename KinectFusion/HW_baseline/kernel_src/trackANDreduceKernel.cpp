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
    TrackData curTrackData;
	

	float temp1[16];
	memcpy(temp1, Ttrack_f, 16*sizeof(float));
	float temp2[16];
	memcpy(temp2, view_f, 16*sizeof(float));
	#pragma HLS ARRAY_PARTITION variable=temp1 complete
	#pragma HLS ARRAY_PARTITION variable=temp2 complete


	COPY_LOOP: for (int i = 0; i < 4; i ++) {
		#pragma HLS PIPELINE II=1
		Ttrack.data[i].x = temp1[i*4];
		Ttrack.data[i].y = temp1[i*4 + 1];
		Ttrack.data[i].z = temp1[i*4 + 2];
		Ttrack.data[i].w = temp1[i*4 + 3];
		view.data[i].x = temp2[i*4];
		view.data[i].y = temp2[i*4 + 1];
		view.data[i].z = temp2[i*4 + 2];
		view.data[i].w = temp2[i*4 + 3];
	}

    RST_LOOP: for(int i=0; i<32; i++){
		#pragma HLS PIPELINE II=1
        reductionoutput[i*8] = 0.0f;
        reductionoutput[i*8+1] = 0.0f;
        reductionoutput[i*8+2] = 0.0f;
        reductionoutput[i*8+3] = 0.0f;
        reductionoutput[i*8+4] = 0.0f;
        reductionoutput[i*8+5] = 0.0f;
        reductionoutput[i*8+6] = 0.0f;
        reductionoutput[i*8+7] = 0.0f;
    }

	uint2 pixel = make_uint2(0, 0);
	unsigned int pixely, pixelx;

	for (pixely = 0; pixely < inSize_y; pixely++) {
		#pragma HLS LOOP_TRIPCOUNT min=60 max=240
        float *sums = reductionoutput + (pixely%8) * 32;
        // in new_reduce, where the reductionoutput is processed in block by 8
		#pragma HLS PIPELINE off
		for (pixelx = 0; pixelx < inSize_x; pixelx++) {
			#pragma HLS LOOP_TRIPCOUNT min=80 max=320
			#pragma HLS PIPELINE off
			pixel.x = pixelx;
			pixel.y = pixely;

			//TrackData & row = output[pixel.x + pixel.y * refSize_x];
			int idx = pixel.x + pixel.y * refSize_x;

			if (inNormal[pixel.x + pixel.y * inSize_x].x == KFUSION_INVALID) {
                curTrackData.result = -1;
				sums[29] += curTrackData.result == -4 ? 1 : 0;
				sums[30] += curTrackData.result == -5 ? 1 : 0;
				sums[31] += curTrackData.result > -4 ? 1 : 0;
				continue;
			}

			float3 projectedVertex = Ttrack
					* make_float3(inVertex[pixel.x + pixel.y * inSize_x]);
			float3 projectedPos = view * projectedVertex;
			float2 projPixel = make_float2(
					projectedPos.x / projectedPos.z + 0.5f,
					projectedPos.y / projectedPos.z + 0.5f);
			if (projPixel.x < 0 || projPixel.x > refSize_x - 1
					|| projPixel.y < 0 || projPixel.y > refSize_y - 1) {
                curTrackData.result = -2;
				sums[29] += curTrackData.result == -4 ? 1 : 0;
				sums[30] += curTrackData.result == -5 ? 1 : 0;
				sums[31] += curTrackData.result > -4 ? 1 : 0;
				continue;
			}

			uint2 refPixel = make_uint2(projPixel.x, projPixel.y);
			float3 referenceNormal;
			referenceNormal.x = refNormal[refPixel.x + refPixel.y * refSize_x].x;
			referenceNormal.y = refNormal[refPixel.x + refPixel.y * refSize_x].y;
			referenceNormal.z = refNormal[refPixel.x + refPixel.y * refSize_x].z;

			if (referenceNormal.x == KFUSION_INVALID) {
                curTrackData.result = -3;
				sums[29] += curTrackData.result == -4 ? 1 : 0;
				sums[30] += curTrackData.result == -5 ? 1 : 0;
				sums[31] += curTrackData.result > -4 ? 1 : 0;
				continue;
			}

			float3 diff = make_float3(refVertex[refPixel.x + refPixel.y * refSize_x])
					- projectedVertex;
			float3 projectedNormal = rotate(Ttrack,
					inNormal[pixel.x + pixel.y * inSize_x]);

			if (length(diff) > dist_threshold) {
                curTrackData.result = -4;
				sums[29] += curTrackData.result == -4 ? 1 : 0;
				sums[30] += curTrackData.result == -5 ? 1 : 0;
				sums[31] += curTrackData.result > -4 ? 1 : 0;
				continue;
			}
			if (dot(projectedNormal, referenceNormal) < normal_threshold) {
                curTrackData.result = -5;
				sums[29] += curTrackData.result == -4 ? 1 : 0;
				sums[30] += curTrackData.result == -5 ? 1 : 0;
				sums[31] += curTrackData.result > -4 ? 1 : 0;
				continue;
			}
            curTrackData.result = 1;

			curTrackData.error = dot(referenceNormal, diff);

			float3 cross_res = cross(projectedVertex, referenceNormal);

			curTrackData.J[0] = referenceNormal.x;
			curTrackData.J[1] = referenceNormal.y;
			curTrackData.J[2] = referenceNormal.z;
			curTrackData.J[3] = cross_res.x;
			curTrackData.J[4] = cross_res.y;
			curTrackData.J[5] = cross_res.z;
            
			sums[0] += curTrackData.error * curTrackData.error;
			sums[1] += curTrackData.error * curTrackData.J[0];
			sums[2] += curTrackData.error * curTrackData.J[1];
			sums[3] += curTrackData.error * curTrackData.J[2];
			sums[4] += curTrackData.error * curTrackData.J[3];
			sums[5] += curTrackData.error * curTrackData.J[4];
			sums[6] += curTrackData.error * curTrackData.J[5];
			sums[7] += curTrackData.J[0] * curTrackData.J[0];
			sums[8] += curTrackData.J[0] * curTrackData.J[1];
			sums[9] += curTrackData.J[0] * curTrackData.J[2];
			sums[10] += curTrackData.J[0] * curTrackData.J[3];
			sums[11] += curTrackData.J[0] * curTrackData.J[4];
			sums[12] += curTrackData.J[0] * curTrackData.J[5];
			sums[13] += curTrackData.J[1] * curTrackData.J[1];
			sums[14] += curTrackData.J[1] * curTrackData.J[2];
			sums[15] += curTrackData.J[1] * curTrackData.J[3];
			sums[16] += curTrackData.J[1] * curTrackData.J[4];
			sums[17] += curTrackData.J[1] * curTrackData.J[5];
			sums[18] += curTrackData.J[2] * curTrackData.J[2];
			sums[19] += curTrackData.J[2] * curTrackData.J[3];
			sums[20] += curTrackData.J[2] * curTrackData.J[4];
			sums[21] += curTrackData.J[2] * curTrackData.J[5];
			sums[22] += curTrackData.J[3] * curTrackData.J[3];
			sums[23] += curTrackData.J[3] * curTrackData.J[4];
			sums[24] += curTrackData.J[3] * curTrackData.J[5];
			sums[25] += curTrackData.J[4] * curTrackData.J[4];
			sums[26] += curTrackData.J[4] * curTrackData.J[5];
            sums[27] += curTrackData.J[5] * curTrackData.J[5];
			sums[28] += 1;


		}
	}
}
}
