/*

 Copyright (c) 2014 University of Edinburgh, Imperial College, University of Manchester.
 Developed in the PAMELA project, EPSRC Programme Grant EP/K008730/1

 This code is licensed under the MIT License.

 */

#ifndef _KERNELS_
#define _KERNELS_

#include <cstdlib>
#include <commons.h>


#ifndef _HW_
#define CL_HPP_CL_1_2_DEFAULT_BUILD
#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_ENABLE_PROGRAM_CONSTRUCTION_FROM_ARRAY_COMPATIBILITY 1
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <vector>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <CL/cl2.hpp>

//OCL_CHECK doesn't work if call has templatized function call
#define OCL_CHECK(error,call)                                       \
    call;                                                           \
    if (error != CL_SUCCESS) {                                      \
      printf("%s:%d Error calling " #call ", error code is: %d\n",  \
              __FILE__,__LINE__, error);                            \
      exit(EXIT_FAILURE);                                           \
    }


	template <typename T>
struct aligned_allocator
{
  using value_type = T;
  T* allocate(std::size_t num)
  {
    void* ptr = nullptr;
    if (posix_memalign(&ptr,4096,num*sizeof(T)))
      throw std::bad_alloc();
    return reinterpret_cast<T*>(ptr);
  }
  void deallocate(T* p, std::size_t num)
  {
    free(p);
  }
};
#endif

#include "hls_half.h"
typedef half floatH;

////////////////////////// COMPUTATION KERNELS PROTOTYPES //////////////////////

void initVolumeKernel(Volume volume);

void bilateralFilterKernel(uint2 inSize,float e_d, int r);

void depth2vertexKernel(float3* vertex, const float * depth, uint2 imageSize,
		const Matrix4 invK);

void reduceKernel(float * out, TrackData* J, const uint2 Jsize,
		const uint2 size);

void trackKernel(float4* output, const float4* inVertex,
		const float4* inNormal, uint2 inSize, const float4* refVertex,
		const float4* refNormal, uint2 refSize, const Matrix4 Ttrack,
		const Matrix4 view, const float dist_threshold,
		const float normal_threshold);

void vertex2normalKernel(float3 * out, const float3 * in, uint2 imageSize);

void mm2metersKernel(float * out, uint2 outSize, const ushort * in,
		uint2 inSize);

void halfSampleRobustImageKernel(float* out, const float* in, uint2 imageSize,
		const float e_d, const int r);

bool updatePoseKernel(Matrix4 & pose, const float * output,
		float icp_threshold);

bool checkPoseKernel(Matrix4 & pose, Matrix4 oldPose, const float * output,
		uint2 imageSize, float track_threshold);

void integrateKernel(Volume vol, const float* depth, uint2 imageSize,
		const Matrix4 invTrack, const Matrix4 K, const float mu,
		const float maxweight);

void raycastKernel(float3* vertex, float3* normal, uint2 inputSize,
		const Volume integration, const Matrix4 view, const float nearPlane,
		const float farPlane, const float step, const float largestep);

////////////////////////// RENDER KERNELS PROTOTYPES //////////////////////

void renderDepthKernel(uchar4* out, float * depth, uint2 depthSize,
		const float nearPlane, const float farPlane);

void renderNormaKernell(uchar3* out, const float3* normal, uint2 normalSize);

void renderTrackKernel(uchar4* out, const TrackData* data, uint2 outSize);

void renderVolumeKernel(uchar4* out, const uint2 depthSize, const Volume volume,
		const Matrix4 view, const float nearPlane, const float farPlane,
		const float step, const float largestep, const float3 light,
		const float3 ambient);

////////////////////////// MULTI-KERNELS PROTOTYPES //////////////////////
void computeFrame(Volume & integration, float3 * vertex, float3 * normal,
		TrackData * trackingResult, Matrix4 & pose, const float * inputDepth,
		const uint2 inputSize, const float * gaussian,
		const std::vector<int> iterations, float4 k, const uint frame);

void init();

void clean();

/// OBJ ///

class Kfusion {
private:
	uint2 inputSize;
	uint2 computationSize;
	float step;
	Matrix4 pose;
	Matrix4 *viewPose;
	float3 volumeDimensions;
	uint3 volumeResolution;
	std::vector<int> iterations;
	bool _tracked;
	bool _integrated;
	float3 _initPose;

	void raycast(uint frame, const float4& k, float mu);

public:
  std::string binaryFile;
	Kfusion(uint2 computationSize, uint3 volumeResolution, float3 volumeDimensions,
			float3 initPose, std::vector<int> & pyramid, std::string binaryFile) {
		
		this->inputSize = make_uint2(inputSize.x, inputSize.y);
		this->computationSize = make_uint2(computationSize.x, computationSize.y);
		this->_initPose = initPose;
		this->volumeDimensions = volumeDimensions;
		this->volumeResolution = volumeResolution;
		pose = toMatrix4(
				TooN::SE3<float>(
						TooN::makeVector(initPose.x, initPose.y, initPose.z, 0,
								0, 0)));
		this->iterations.clear();
		for (std::vector<int>::iterator it = pyramid.begin();
				it != pyramid.end(); it++) {
			this->iterations.push_back(*it);
		}

		step = min(volumeDimensions) / max(volumeResolution);
		viewPose = &pose;
   this->binaryFile = binaryFile;
		this->languageSpecificConstructor();
	}
//Allow a kfusion object to be created with a pose which include orientation as well as position
	Kfusion(uint2 computationSize, uint3 volumeResolution, float3 volumeDimensions,
			Matrix4 initPose, std::vector<int> & pyramid, std::string binaryFile) {
		
		this->inputSize = make_uint2(inputSize.x, inputSize.y);
		this->computationSize = make_uint2(computationSize.x, computationSize.y);
		this->_initPose = getPosition();
		this->volumeDimensions = volumeDimensions;
		this->volumeResolution = volumeResolution;
		pose = initPose;

		this->iterations.clear();
		for (std::vector<int>::iterator it = pyramid.begin();
				it != pyramid.end(); it++) {
			this->iterations.push_back(*it);
		}

		step = min(volumeDimensions) / max(volumeResolution);
		viewPose = &pose;
    this->binaryFile = binaryFile;
		this->languageSpecificConstructor();
	}

	void languageSpecificConstructor();
	~Kfusion();

	void reset();
	bool getTracked() {
		return (_tracked);
	}
	bool getIntegrated() {
		return (_integrated);
	}
	float3 getPosition() {
		//std::cerr << "InitPose =" << _initPose.x << "," << _initPose.y  <<"," << _initPose.z << "    ";
		//std::cerr << "pose =" << pose.data[0].w << "," << pose.data[1].w  <<"," << pose.data[2].w << "    ";
		float xt = pose.data[0].w - _initPose.x;
		float yt = pose.data[1].w - _initPose.y;
		float zt = pose.data[2].w - _initPose.z;
		return (make_float3(xt, yt, zt));
	}
	void computeFrame(const ushort * inputDepth, const uint2 inputSize,
			float4 k, uint integration_rate, uint tracking_rate,
			  float icp_threshold, float mu, const uint frame) ;

	bool preprocessing(const ushort * inputDepth, const uint2 inputSize);
	bool tracking(float4 k, float icp_threshold, uint tracking_rate,
			uint frame);
	bool raycasting(float4 k, float mu, uint frame);
	bool integration(float4 k, uint integration_rate, float mu, uint frame);

	void dumpVolume(const char* filename);
	void renderVolume(uchar4 * out, const uint2 outputSize, int frame, int rate,
			float4 k, float mu);
	void renderTrack(uchar4 * out, const uint2 outputSize);
	void renderDepth(uchar4* out, uint2 outputSize);
	Matrix4 getPose() {
		return pose;
	}
	void setViewPose(Matrix4 *value = NULL) {
		if (value == NULL)
			viewPose = &pose;
		else
			viewPose = value;
	}
	Matrix4 *getViewPose() {
		return (viewPose);
	}
	float3 getModelDimensions() {
		return (volumeDimensions);
	}
	uint3 getModelResolution() {
		return (volumeResolution);
	}
	uint2 getComputationResolution() {
		return (computationSize);
	}
};

void synchroniseDevices(); // Synchronise CPU and GPU

#endif
