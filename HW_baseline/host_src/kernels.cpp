/*

 Copyright (c) 2014 University of Edinburgh, Imperial College, University of Manchester.
 Developed in the PAMELA project, EPSRC Programme Grant EP/K008730/1

 Also modified by Computer Systems Lab - University of Thessaly.

 Copyright (c) 2021 Computer Systems Lab - University of Thessaly

 Also modified by Lee, Cheng-Hao - 2023 ITRI EOSL summer intern program.

 Copyright (c) 2023 ITRI EOSL

 This code is licensed under the MIT License.

 */

#include <kernels.hpp>
#include <string.h>
#include <omp.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>



//#define OLDREDUCE

#define KERNEL_TIMINGS
#ifdef KERNEL_TIMINGS
FILE *kernel_timings_log;
#endif

#define NUM_LEVELS 3

struct timespec tick_clockData;
struct timespec tock_clockData;
	
#define TICK()    {if (print_kernel_timing) {clock_gettime(CLOCK_MONOTONIC, &tick_clockData);}}

#ifndef KERNEL_TIMINGS
#define TOCK(str,size)  {if (print_kernel_timing) {clock_gettime(CLOCK_MONOTONIC, &tock_clockData); std::cerr<< str << " ";\
	if((tock_clockData.tv_sec > tick_clockData.tv_sec) && (tock_clockData.tv_nsec >= tick_clockData.tv_nsec))   std::cerr<< tock_clockData.tv_sec - tick_clockData.tv_sec << std::setfill('0') << std::setw(9);\
	std::cerr  << (( tock_clockData.tv_nsec - tick_clockData.tv_nsec) + ((tock_clockData.tv_nsec<tick_clockData.tv_nsec)?1000000000:0)) << " " <<  size << std::endl;}}
#else
#define TOCK(str,size) { if(print_kernel_timing) { clock_gettime(CLOCK_MONOTONIC, &tock_clockData);\
							  fprintf(kernel_timings_log,"%s\t%d\t%f\t%f\n",str,size,(double) tick_clockData.tv_sec + tick_clockData.tv_nsec / 1000000000.0,(double) tock_clockData.tv_sec + tock_clockData.tv_nsec / 1000000000.0); }}
#endif
inline double tock() {

		struct timespec clockData;
		clock_gettime(CLOCK_MONOTONIC, &clockData);

		return (double) clockData.tv_sec + clockData.tv_nsec / 1000000000.0;
}	

cl_uint load_file_to_memory(const char *filename, char **result)
{
    cl_uint size = 0;
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        *result = NULL;
        return -1; // -1 means file opening fail
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    *result = (char *)malloc(size+1);
    if (size != fread(*result, sizeof(char), size, f)) {
        free(*result);
        return -2; // -2 means file reading fail
    }
    fclose(f);
    (*result)[size] = 0;
    return size;
}


// inter-frame
Volume volume;

float4 *vertexF4;
float4 *normalF4;

// intra-frame

Matrix4 oldPose;
Matrix4 raycastPose;

bool print_kernel_timing = true;
//////////////////// Variables for build up OpenCL environment //////////
cl_int err;
cl_int status;

cl_platform_id platform_id;       
cl_platform_id platforms[16];       
cl_uint platform_count;
cl_uint platform_found = 0;
char cl_platform_vendor[1001];

cl_uint num_devices;
cl_uint device_found = 0;
cl_device_id devices[16];  
char cl_device_name[1001];
cl_device_id device_id;

cl_context context;
cl_command_queue Q, Q_bilateralFilterKernel, Q_halfSampleDepth2vertexKernel, Q_vertex2normalKernel, Q_trackANDreduceKernel, Q_integrateKernel;
cl_program program;
cl_kernel K_bilateralFilterKernel, K_mm2meter2bilateralFilterKernel, K_halfSampleDepth2vertexKernel, K_vertex2normalKernel, K_trackANDreduceKernel, K_integrateKernel;
/////////////////////////////////////////////////////////////////////////

// bilateralFilterKernel
ushort* inputDepth;
float * floatDepth;
float ** scaledDepth;
float * gaussian;

cl_mem inputDepth_BUF;
cl_mem floatDepth_BUF;
cl_mem scaledDepth0_BUF;
cl_mem gaussian_BUF;


// trackKernel
float* reductionoutput;
float4 ** inputVertex;
float4 ** inputNormal;
float4* vertex;
float4* normal;
float* Ttrack;
float* view;

cl_mem reductionoutput_BUF;
cl_mem inputVertex_BUF[NUM_LEVELS];
cl_mem inputNormal_BUF[NUM_LEVELS];
cl_mem vertex_BUF;
cl_mem normal_BUF;
cl_mem Ttrack_BUF;
cl_mem view_BUF;


// integrate kernel variables
short2 *volumeData;
uint4* volumeSize;
float4* volumeDim;
float* inv_Ttrack;
float *cameraK;

cl_mem volumeData_BUF;
cl_mem volumeSize_BUF;
cl_mem volumeDim_BUF;
cl_mem inv_Ttrack_BUF;
cl_mem cameraK_BUF;

cl_mem pt_bf[3];

cl_mem pt_in[6];
cl_mem pt_out[1];

cl_mem pt_tr_in[6];
cl_mem pt_tr_out[2];

void Kfusion::languageSpecificConstructor() {

	if (getenv("KERNEL_TIMINGS"))
		print_kernel_timing = true;
	#ifdef KERNEL_TIMINGS
		print_kernel_timing = true;
		kernel_timings_log = fopen("kernel_timings.log","w");
	#endif
	/**********************************************
	 * 
	 * 			Xilinx OpenCL Initialization 
	 * 
	 * *********************************************/
	// Find Platform and Devices
	OCL_CHECK(err, err = clGetPlatformIDs(16, platforms, &platform_count));
	printf("INFO: Found %d platforms\n", platform_count);
	for (cl_uint iplat=0; iplat<platform_count; iplat++) {
		OCL_CHECK(err, err = clGetPlatformInfo(platforms[iplat], CL_PLATFORM_VENDOR, 1000, (void *)cl_platform_vendor,NULL));
		if (strcmp(cl_platform_vendor, "Xilinx") == 0) {
			printf("INFO: Selected platform %d from %s\n", iplat, cl_platform_vendor);
			platform_id = platforms[iplat];
			platform_found = 1;
		}
		else{
			printf("ERROR: Platform Xilinx not found. Exit.\n");
			exit(EXIT_FAILURE);
		}
	}
	OCL_CHECK(err, err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ACCELERATOR, 16, devices, &num_devices));
	printf("INFO: Found %d devices\n", num_devices);
	device_id = devices[0]; // we have only one device

	// Create Context
	OCL_CHECK(err, context = clCreateContext(0,1,&device_id,NULL,NULL, &err));
	if (!context) {
		printf("Error: Failed to create a compute context!\n");
		printf("Test failed\n");
		exit(EXIT_FAILURE);
	}

	// Create Command Queue
	OCL_CHECK(err, Q = clCreateCommandQueue(context, device_id, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE|CL_QUEUE_PROFILING_ENABLE, &err));
	OCL_CHECK(err, Q_bilateralFilterKernel = clCreateCommandQueue(context, device_id, CL_QUEUE_PROFILING_ENABLE, &err));
	OCL_CHECK(err, Q_halfSampleDepth2vertexKernel = clCreateCommandQueue(context, device_id, CL_QUEUE_PROFILING_ENABLE, &err));
	OCL_CHECK(err, Q_vertex2normalKernel = clCreateCommandQueue(context, device_id, CL_QUEUE_PROFILING_ENABLE, &err));
	OCL_CHECK(err, Q_trackANDreduceKernel = clCreateCommandQueue(context, device_id, CL_QUEUE_PROFILING_ENABLE, &err));
	OCL_CHECK(err, Q_integrateKernel = clCreateCommandQueue(context, device_id, CL_QUEUE_PROFILING_ENABLE, &err));

	// Load Binary File from disk
	unsigned char *kernelbinary;
	char *xclbin = (char*)binaryFile.c_str();
	cl_uint n_i0 = load_file_to_memory(xclbin, (char **) &kernelbinary);
	if (n_i0 < 0) {
		printf("failed to load kernel from xclbin: %s\n", xclbin);
		printf("Test failed\n");
		exit(EXIT_FAILURE);    
	}
	size_t n0 = n_i0;
	OCL_CHECK(err, program = clCreateProgramWithBinary(context, 1, &device_id, &n0, (const unsigned char**) &kernelbinary, &status, &err));
	free(kernelbinary);

	// Create Program
	OCL_CHECK(err, err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL));
	
	// Create Kernels
	OCL_CHECK(err, K_bilateralFilterKernel = clCreateKernel(program, "bilateralFilterKernel", &err));
	OCL_CHECK(err, K_halfSampleDepth2vertexKernel = clCreateKernel(program, "halfSampleDepth2vertexKernel", &err));
	OCL_CHECK(err, K_vertex2normalKernel = clCreateKernel(program, "vertex2normalKernel", &err));
	OCL_CHECK(err, K_integrateKernel = clCreateKernel(program, "integrateKernel", &err));
	OCL_CHECK(err, K_trackANDreduceKernel = clCreateKernel(program, "trackANDreduceKernel", &err));

	// internal buffers to initialize
	// reductionoutput = (float*) calloc(sizeof(float) * 8 * 32, 1);
	scaledDepth = (float**) calloc(sizeof(float*) * iterations.size(), 1);
	inputVertex = (float4**) calloc(sizeof(float4*) * iterations.size(), 1);
	inputNormal = (float4**) calloc(sizeof(float4*) * iterations.size(), 1);

	// opencl out memory size
	size_t compSize = computationSize.x * computationSize.y;
	size_t gaussianSize = radius * 2 + 1;
	
	
	// bilateralFilterKernel(scaledDepth[0], floatDepth, (computationSize), gaussian, (e_delta), (radius));
	// OCL_CHECK(err, inputDepth_BUF = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(ushort)*inputSize.x*inputSize.y, NULL, &err));
	OCL_CHECK(err, scaledDepth0_BUF = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(float)*compSize, NULL, &err));
	OCL_CHECK(err, floatDepth_BUF = clCreateBuffer(context,  CL_MEM_READ_ONLY,  sizeof(float)*compSize, NULL, &err));
	OCL_CHECK(err, gaussian_BUF = clCreateBuffer(context,  CL_MEM_READ_ONLY,  sizeof(float)*gaussianSize, NULL, &err));
	
	// OCL_CHECK(err, inputDepth = (ushort *)clEnqueueMapBuffer(Q,inputDepth_BUF,true,CL_MAP_READ,0, sizeof(ushort)*inputSize.x*inputSize.y,0,nullptr,nullptr,&err));
	OCL_CHECK(err, scaledDepth[0] = (float *)clEnqueueMapBuffer(Q,scaledDepth0_BUF,true,CL_MAP_READ,0, sizeof(float)*compSize,0,nullptr,nullptr,&err));
	OCL_CHECK(err, floatDepth = (float *)clEnqueueMapBuffer(Q,floatDepth_BUF,true,CL_MAP_WRITE,0,sizeof(float)*compSize,0,nullptr,nullptr,&err));
	OCL_CHECK(err, gaussian = (float *)clEnqueueMapBuffer(Q,gaussian_BUF,true,CL_MAP_WRITE,0,sizeof(float)*gaussianSize,0,nullptr,nullptr,&err));
	
	//halfSampleDepth2vertexKernel(inputVertex[0], inputVertex[1], inputVertex[2], scaledDepth[0], inSize_x, inSize_y, e_d, r, K)


	// trackKernel(trackingResult, inputVertex[level], inputNormal[level], (localimagesize), vertex, normal, (computationSize), pose, projectReference, (dist_threshold), (normal_threshold));
	OCL_CHECK(err, reductionoutput_BUF = clCreateBuffer(context, CL_MEM_READ_WRITE,  sizeof(float) * 8 * 32, NULL, &err));
	for (int i = 0; i < iterations.size(); i++) {
	size_t compLevelSize = sizeof(float4) * compSize / (int) pow(2, i);
	OCL_CHECK(err, inputVertex_BUF[i] = clCreateBuffer(context, CL_MEM_READ_WRITE, compLevelSize, NULL, &err));
	OCL_CHECK(err, inputNormal_BUF[i] = clCreateBuffer(context, CL_MEM_READ_WRITE, compLevelSize, NULL, &err));
	}
	OCL_CHECK(err, vertex_BUF = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float4)*compSize, NULL, &err));
	OCL_CHECK(err, normal_BUF = clCreateBuffer(context, CL_MEM_READ_WRITE,  sizeof(float4)*compSize, NULL, &err));
	OCL_CHECK(err, Ttrack_BUF = clCreateBuffer(context, CL_MEM_READ_ONLY, 16*sizeof(float), NULL, &err));
	OCL_CHECK(err, view_BUF = clCreateBuffer(context, CL_MEM_READ_ONLY, 16*sizeof(float), NULL, &err));
	
	OCL_CHECK(err, reductionoutput = (float *)clEnqueueMapBuffer(Q,reductionoutput_BUF,true,CL_MAP_READ | CL_MAP_WRITE,0,sizeof(float) * 8 * 32,0,nullptr,nullptr,&err));
	for (int i = 0; i < iterations.size(); i++) {
	size_t compLevelSize = sizeof(float4) * compSize / (int) pow(2, i);
	OCL_CHECK(err, inputVertex[i] = (float4*)clEnqueueMapBuffer(Q, inputVertex_BUF[i], true, CL_MAP_READ | CL_MAP_WRITE, 0, compLevelSize, 0, nullptr, nullptr, &err));
	OCL_CHECK(err, inputNormal[i] = (float4*)clEnqueueMapBuffer(Q, inputNormal_BUF[i], true, CL_MAP_READ | CL_MAP_WRITE, 0, compLevelSize, 0, nullptr, nullptr, &err));
	}
	OCL_CHECK(err, vertex = (float4*)clEnqueueMapBuffer(Q, vertex_BUF, true, CL_MAP_READ | CL_MAP_WRITE, 0,  sizeof(float4)*compSize, 0, nullptr, nullptr, &err));
	OCL_CHECK(err, normal = (float4*)clEnqueueMapBuffer(Q, normal_BUF, true, CL_MAP_READ | CL_MAP_WRITE, 0,  sizeof(float4)*compSize, 0, nullptr, nullptr, &err));
	OCL_CHECK(err, Ttrack = (float*)clEnqueueMapBuffer(Q,Ttrack_BUF,true,CL_MAP_WRITE,0,16*sizeof(float),0,nullptr,nullptr,&err));
	OCL_CHECK(err, view = (float*)clEnqueueMapBuffer(Q,view_BUF,true,CL_MAP_WRITE,0,16*sizeof(float),0,nullptr,nullptr,&err));
	

	// integrateKernel(volume, floatDepth, (computationSize), inverse(pose), getCameraMatrix(k), (mu), (maxweight));
	// struct Volume{uint3 size; float3 dim; short2 * data;}
	size_t volResSize = volumeResolution.x * volumeResolution.y *volumeResolution.z;
	
	OCL_CHECK(err, volumeData_BUF = clCreateBuffer(context,  CL_MEM_READ_WRITE,  sizeof(short2)*volResSize, NULL, &err));
	OCL_CHECK(err, volumeSize_BUF = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(uint4), NULL, &err));
	OCL_CHECK(err, volumeDim_BUF = clCreateBuffer(context, CL_MEM_READ_ONLY,  sizeof(float4), NULL, &err));
	OCL_CHECK(err, inv_Ttrack_BUF = clCreateBuffer(context, CL_MEM_READ_ONLY, 16*sizeof(float), NULL, &err));
	OCL_CHECK(err, cameraK_BUF = clCreateBuffer(context, CL_MEM_READ_ONLY, 16*sizeof(float), NULL, &err));
	
	OCL_CHECK(err, volumeData = (short2 *)clEnqueueMapBuffer(Q,volumeData_BUF,true,CL_MAP_READ | CL_MAP_WRITE,0,sizeof(short2)*volResSize,0,nullptr,nullptr,&err));
	OCL_CHECK(err, volumeSize = (uint4 *)clEnqueueMapBuffer(Q,volumeSize_BUF,true,CL_MAP_WRITE,0,sizeof(uint4),0,nullptr,nullptr,&err));
	OCL_CHECK(err, volumeDim = (float4 *)clEnqueueMapBuffer(Q,volumeDim_BUF,true,CL_MAP_WRITE,0,sizeof(float4),0,nullptr,nullptr,&err));
	OCL_CHECK(err, inv_Ttrack = (float *)clEnqueueMapBuffer(Q,inv_Ttrack_BUF,true,CL_MAP_WRITE,0,16*sizeof(float),0,nullptr,nullptr,&err));
	OCL_CHECK(err, cameraK = (float *)clEnqueueMapBuffer(Q,cameraK_BUF,true,CL_MAP_WRITE,0,16*sizeof(float),0,nullptr,nullptr,&err));
	


	for (unsigned int i = 1; i < iterations.size(); ++i) {
		scaledDepth[i] = (float*) calloc(sizeof(float) * (computationSize.x * computationSize.y)/(int) pow(2, i), 1);
	}
	// Generate the gaussian coefficient array
	int x;
	for (unsigned int i = 0; i < gaussianSize; i++) {
		x = i - 2;
		gaussian[i] = expf(-(x * x) / (2 * delta * delta));
	}

	// trackingResult = (TrackData*) calloc(sizeof(TrackData) * computationSize.x * computationSize.y, 1);



	clFinish(Q);
	volume.init(volumeResolution, volumeDimensions,volumeData);

	
	reset();
}

// destructor
Kfusion::~Kfusion() {
#ifdef KERNEL_TIMINGS
	fclose(kernel_timings_log);
#endif
	// Cleanup OpenCL objects
	for (int i = 0; i < iterations.size(); i++) {
		OCL_CHECK(err, err = clEnqueueUnmapMemObject(Q,inputVertex_BUF[i],inputVertex[i],0,NULL,NULL));
		OCL_CHECK(err, err = clEnqueueUnmapMemObject(Q,inputNormal_BUF[i],inputNormal[i],0,NULL,NULL));
	}

	OCL_CHECK(err, err = clEnqueueUnmapMemObject(Q,vertex_BUF,vertex,0,NULL,NULL));
	OCL_CHECK(err, err = clEnqueueUnmapMemObject(Q,normal_BUF,normal,0,NULL,NULL));
	OCL_CHECK(err, err = clEnqueueUnmapMemObject(Q,Ttrack_BUF,Ttrack,0,NULL,NULL));
	OCL_CHECK(err, err = clEnqueueUnmapMemObject(Q,view_BUF,view,0,NULL,NULL));

	clReleaseMemObject(vertex_BUF);
	clReleaseMemObject(normal_BUF);

	clReleaseMemObject(Ttrack_BUF);
	clReleaseMemObject(view_BUF);

	
	OCL_CHECK(err, err = clEnqueueUnmapMemObject(Q,floatDepth_BUF,floatDepth,0,NULL,NULL));
	OCL_CHECK(err, err = clEnqueueUnmapMemObject(Q,gaussian_BUF,gaussian,0,NULL,NULL));
	OCL_CHECK(err, err = clEnqueueUnmapMemObject(Q,scaledDepth0_BUF,scaledDepth[0],0,NULL,NULL));
	clReleaseMemObject(floatDepth_BUF);
	clReleaseMemObject(gaussian_BUF);
	clReleaseMemObject(scaledDepth0_BUF);

	OCL_CHECK(err, err = clEnqueueUnmapMemObject(Q,volumeData_BUF,volumeData,0,NULL,NULL));
	OCL_CHECK(err, err = clEnqueueUnmapMemObject(Q,volumeSize_BUF,volumeSize,0,NULL,NULL));
	OCL_CHECK(err, err = clEnqueueUnmapMemObject(Q,volumeDim_BUF,volumeDim,0,NULL,NULL));
	OCL_CHECK(err, err = clEnqueueUnmapMemObject(Q,inv_Ttrack_BUF,inv_Ttrack,0,NULL,NULL));
	OCL_CHECK(err, err = clEnqueueUnmapMemObject(Q,cameraK_BUF,cameraK,0,NULL,NULL));
	

	clReleaseMemObject(volumeSize_BUF);
	clReleaseMemObject(volumeDim_BUF);
	clReleaseMemObject(inv_Ttrack_BUF);
	clReleaseMemObject(cameraK_BUF);

	clReleaseMemObject(volumeData_BUF);

	clReleaseProgram(program);
    clReleaseKernel(K_bilateralFilterKernel);
	clReleaseKernel(K_halfSampleDepth2vertexKernel);
	clReleaseKernel(K_vertex2normalKernel);
    clReleaseKernel(K_trackANDreduceKernel);
    clReleaseKernel(K_integrateKernel);


    clReleaseCommandQueue(Q);
    clReleaseCommandQueue(Q_bilateralFilterKernel);
	clReleaseCommandQueue(Q_halfSampleDepth2vertexKernel);
	clReleaseCommandQueue(Q_vertex2normalKernel);
    clReleaseCommandQueue(Q_trackANDreduceKernel);
    clReleaseCommandQueue(Q_integrateKernel);
    clReleaseContext(context);

	free(floatDepth);
	// free(trackingResult);

	// free(reductionoutput);
	for (unsigned int i = 0; i < iterations.size(); ++i) {
		if(i!=0){
			free(scaledDepth[i]);
		}
	}
	free(scaledDepth);
	free(inputVertex);
	free(inputNormal);

	volume.release();

}

void Kfusion::reset() {
	initVolumeKernel(volume);
}
void init() {
}
;
// stub
void clean() {
}
;
// stub

void initVolumeKernel(Volume volume) {
	TICK();
	for (unsigned int x = 0; x < volume.size.x; x++)
		for (unsigned int y = 0; y < volume.size.y; y++) {
			for (unsigned int z = 0; z < volume.size.z; z++) {
				//std::cout <<  x << " " << y << " " << z <<"\n";
				volume.setints(x, y, z, make_float2(1.0f, 0.0f));
			}
		}
	TOCK("initVolumeKernel", volume.size.x * volume.size.y * volume.size.z);
}

void bilateralFilterKernel(float* out, float* in, uint size_x, uint size_y,
		const float * gaussian, float e_d, int r){

	TICK();

	pt_bf[0] = floatDepth_BUF;
	pt_bf[1] = gaussian_BUF;
	pt_bf[2] = scaledDepth0_BUF;

	// Set args
	int argcount = 0;
	OCL_CHECK(err, err = clSetKernelArg(K_bilateralFilterKernel, argcount++, sizeof(cl_mem), &scaledDepth0_BUF));
	OCL_CHECK(err, err = clSetKernelArg(K_bilateralFilterKernel, argcount++, sizeof(cl_mem), &floatDepth_BUF));
	OCL_CHECK(err, err = clSetKernelArg(K_bilateralFilterKernel, argcount++, sizeof(uint),&size_x));
	OCL_CHECK(err, err = clSetKernelArg(K_bilateralFilterKernel, argcount++, sizeof(uint),&size_y));
	OCL_CHECK(err, err = clSetKernelArg(K_bilateralFilterKernel, argcount++, sizeof(cl_mem), &gaussian_BUF));
	OCL_CHECK(err, err = clSetKernelArg(K_bilateralFilterKernel, argcount++, sizeof(float), &e_d));
	OCL_CHECK(err, err = clSetKernelArg(K_bilateralFilterKernel, argcount++, sizeof(int), &r));

	// Migrate memory objects and enqueue task
	OCL_CHECK(err, err = clEnqueueMigrateMemObjects(Q_bilateralFilterKernel,(cl_uint)2,pt_bf, 0 ,0,NULL, NULL));
	OCL_CHECK(err, err = clEnqueueTask(Q_bilateralFilterKernel, K_bilateralFilterKernel, 0, NULL, NULL));
	OCL_CHECK(err, err = clEnqueueMigrateMemObjects(Q_bilateralFilterKernel,(cl_uint)1,&pt_bf[2], CL_MIGRATE_MEM_OBJECT_HOST,0,NULL, NULL));

	// Launch the commandqueue
	clFinish(Q_bilateralFilterKernel);

	TOCK("bilateralFilterKernel", size_x * size_y);
}

void depth2vertexKernel(float4* vertex, const float * depth, uint2 imageSize,
		const Matrix4 invK) {
	TICK();
	unsigned int x, y;
	#pragma omp parallel for \
         shared(vertex), private(x, y)
	for (y = 0; y < imageSize.y; y++) {
		for (x = 0; x < imageSize.x; x++) {

			if (depth[x + y * imageSize.x] > 0) {
				vertex[x + y * imageSize.x] = make_float4(depth[x + y * imageSize.x]
						* (rotate(invK, make_float3(x, y, 1.f))));
			} else {
				vertex[x + y * imageSize.x] = make_float4(0);
			}
		}
	}
	TOCK("depth2vertexKernel", imageSize.x * imageSize.y);
}

void vertex2normalKernel(float4 * out, const float4 * in, uint2 imageSize) {
	TICK();
	unsigned int x, y;
	#pragma omp parallel for \
        shared(out), private(x,y)
	for (y = 0; y < imageSize.y; y++) {
		for (x = 0; x < imageSize.x; x++) {
			const uint2 pleft = make_uint2(max(int(x) - 1, 0), y);
			const uint2 pright = make_uint2(min(x + 1, (int) imageSize.x - 1),
					y);
			const uint2 pup = make_uint2(x, max(int(y) - 1, 0));
			const uint2 pdown = make_uint2(x,
					min(y + 1, ((int) imageSize.y) - 1));

			const float4 left = in[pleft.x + imageSize.x * pleft.y];
			const float4 right = in[pright.x + imageSize.x * pright.y];
			const float4 up = in[pup.x + imageSize.x * pup.y];
			const float4 down = in[pdown.x + imageSize.x * pdown.y];

			if (left.z == 0 || right.z == 0 || up.z == 0 || down.z == 0) {
				out[x + y * imageSize.x].x = KFUSION_INVALID;
				continue;
			}
			const float3 dxv = make_float3(right - left);
			const float3 dyv = make_float3(down - up);
			out[x + y * imageSize.x] = make_float4(normalize(cross(dyv, dxv))); // switched dx and dy to get factor -1
		}
	}
	TOCK("vertex2normalKernel", imageSize.x * imageSize.y);
}

void new_reduce(int blockIndex, float * out, TrackData* J, const uint2 Jsize,
		const uint2 size) {
	float *sums = out + blockIndex * 32;

	float * jtj = sums + 7;
	float * info = sums + 28;
	for (uint i = 0; i < 32; ++i)
		sums[i] = 0;
	float sums0, sums1, sums2, sums3, sums4, sums5, sums6, sums7, sums8, sums9,
			sums10, sums11, sums12, sums13, sums14, sums15, sums16, sums17,
			sums18, sums19, sums20, sums21, sums22, sums23, sums24, sums25,
			sums26, sums27, sums28, sums29, sums30, sums31;
	sums0 = 0.0f;
	sums1 = 0.0f;
	sums2 = 0.0f;
	sums3 = 0.0f;
	sums4 = 0.0f;
	sums5 = 0.0f;
	sums6 = 0.0f;
	sums7 = 0.0f;
	sums8 = 0.0f;
	sums9 = 0.0f;
	sums10 = 0.0f;
	sums11 = 0.0f;
	sums12 = 0.0f;
	sums13 = 0.0f;
	sums14 = 0.0f;
	sums15 = 0.0f;
	sums16 = 0.0f;
	sums17 = 0.0f;
	sums18 = 0.0f;
	sums19 = 0.0f;
	sums20 = 0.0f;
	sums21 = 0.0f;
	sums22 = 0.0f;
	sums23 = 0.0f;
	sums24 = 0.0f;
	sums25 = 0.0f;
	sums26 = 0.0f;
	sums27 = 0.0f;
	sums28 = 0.0f;
	sums29 = 0.0f;
	sums30 = 0.0f;
	sums31 = 0.0f;
	// comment me out to try coarse grain parallelism 
	#pragma omp parallel for reduction(+:sums0,sums1,sums2,sums3,sums4,sums5,sums6,sums7,sums8,sums9,sums10,sums11,sums12,sums13,sums14,sums15,sums16,sums17,sums18,sums19,sums20,sums21,sums22,sums23,sums24,sums25,sums26,sums27,sums28,sums29,sums30,sums31)
	for (uint y = blockIndex; y < size.y; y += 8) {
		for (uint x = 0; x < size.x; x++) {

			const TrackData & row = J[(x + y * Jsize.x)]; // ...
			if (row.result < 1) {
				// accesses sums[28..31]
				/*(sums+28)[1]*/sums29 += row.result == -4 ? 1 : 0;
				/*(sums+28)[2]*/sums30 += row.result == -5 ? 1 : 0;
				/*(sums+28)[3]*/sums31 += row.result > -4 ? 1 : 0;

				continue;
			}
			// Error part
			/*sums[0]*/sums0 += row.error * row.error;

			// JTe part
			/*for(int i = 0; i < 6; ++i)
			 sums[i+1] += row.error * row.J[i];*/
			sums1 += row.error * row.J[0];
			sums2 += row.error * row.J[1];
			sums3 += row.error * row.J[2];
			sums4 += row.error * row.J[3];
			sums5 += row.error * row.J[4];
			sums6 += row.error * row.J[5];

			// JTJ part, unfortunatly the double loop is not unrolled well...
			/*(sums+7)[0]*/sums7 += row.J[0] * row.J[0];
			/*(sums+7)[1]*/sums8 += row.J[0] * row.J[1];
			/*(sums+7)[2]*/sums9 += row.J[0] * row.J[2];
			/*(sums+7)[3]*/sums10 += row.J[0] * row.J[3];

			/*(sums+7)[4]*/sums11 += row.J[0] * row.J[4];
			/*(sums+7)[5]*/sums12 += row.J[0] * row.J[5];

			/*(sums+7)[6]*/sums13 += row.J[1] * row.J[1];
			/*(sums+7)[7]*/sums14 += row.J[1] * row.J[2];
			/*(sums+7)[8]*/sums15 += row.J[1] * row.J[3];
			/*(sums+7)[9]*/sums16 += row.J[1] * row.J[4];

			/*(sums+7)[10]*/sums17 += row.J[1] * row.J[5];

			/*(sums+7)[11]*/sums18 += row.J[2] * row.J[2];
			/*(sums+7)[12]*/sums19 += row.J[2] * row.J[3];
			/*(sums+7)[13]*/sums20 += row.J[2] * row.J[4];
			/*(sums+7)[14]*/sums21 += row.J[2] * row.J[5];

			/*(sums+7)[15]*/sums22 += row.J[3] * row.J[3];
			/*(sums+7)[16]*/sums23 += row.J[3] * row.J[4];
			/*(sums+7)[17]*/sums24 += row.J[3] * row.J[5];

			/*(sums+7)[18]*/sums25 += row.J[4] * row.J[4];
			/*(sums+7)[19]*/sums26 += row.J[4] * row.J[5];

			/*(sums+7)[20]*/sums27 += row.J[5] * row.J[5];

			// extra info here
			/*(sums+28)[0]*/sums28 += 1;

		}
	}
	sums[0] = sums0;
	sums[1] = sums1;
	sums[2] = sums2;
	sums[3] = sums3;
	sums[4] = sums4;
	sums[5] = sums5;
	sums[6] = sums6;
	sums[7] = sums7;
	sums[8] = sums8;
	sums[9] = sums9;
	sums[10] = sums10;
	sums[11] = sums11;
	sums[12] = sums12;
	sums[13] = sums13;
	sums[14] = sums14;
	sums[15] = sums15;
	sums[16] = sums16;
	sums[17] = sums17;
	sums[18] = sums18;
	sums[19] = sums19;
	sums[20] = sums20;
	sums[21] = sums21;
	sums[22] = sums22;
	sums[23] = sums23;
	sums[24] = sums24;
	sums[25] = sums25;
	sums[26] = sums26;
	sums[27] = sums27;
	sums[28] = sums28;
	sums[29] = sums29;
	sums[30] = sums30;
	sums[31] = sums31;

}
void reduceKernel(float * out, TrackData* J, const uint2 Jsize,
		const uint2 size) {
	TICK();
	int blockIndex;
	#ifdef OLDREDUCE
	#pragma omp parallel for private (blockIndex)
	#endif
	for (blockIndex = 0; blockIndex < 8; blockIndex++) {

	#ifdef OLDREDUCE
		float S[112][32]; // this is for the final accumulation
		// we have 112 threads in a blockdim
		// and 8 blocks in a gridDim?
		// ie it was launched as <<<8,112>>>
		uint sline;// threadIndex.x
		float sums[32];

		for(int threadIndex = 0; threadIndex < 112; threadIndex++) {
			sline = threadIndex;
			float * jtj = sums+7;
			float * info = sums+28;
			for(uint i = 0; i < 32; ++i) sums[i] = 0;

			for(uint y = blockIndex; y < size.y; y += 8 /*gridDim.x*/) {
				for(uint x = sline; x < size.x; x += 112 /*blockDim.x*/) {
					const TrackData & row = J[(x + y * Jsize.x)]; // ...

					if(row.result < 1) {
						// accesses S[threadIndex][28..31]
						info[1] += row.result == -4 ? 1 : 0;
						info[2] += row.result == -5 ? 1 : 0;
						info[3] += row.result > -4 ? 1 : 0;
						continue;
					}
					// Error part
					sums[0] += row.error * row.error;

					// JTe part
					for(int i = 0; i < 6; ++i)
					sums[i+1] += row.error * row.J[i];

					// JTJ part, unfortunatly the double loop is not unrolled well...
					jtj[0] += row.J[0] * row.J[0];
					jtj[1] += row.J[0] * row.J[1];
					jtj[2] += row.J[0] * row.J[2];
					jtj[3] += row.J[0] * row.J[3];

					jtj[4] += row.J[0] * row.J[4];
					jtj[5] += row.J[0] * row.J[5];

					jtj[6] += row.J[1] * row.J[1];
					jtj[7] += row.J[1] * row.J[2];
					jtj[8] += row.J[1] * row.J[3];
					jtj[9] += row.J[1] * row.J[4];

					jtj[10] += row.J[1] * row.J[5];

					jtj[11] += row.J[2] * row.J[2];
					jtj[12] += row.J[2] * row.J[3];
					jtj[13] += row.J[2] * row.J[4];
					jtj[14] += row.J[2] * row.J[5];

					jtj[15] += row.J[3] * row.J[3];
					jtj[16] += row.J[3] * row.J[4];
					jtj[17] += row.J[3] * row.J[5];

					jtj[18] += row.J[4] * row.J[4];
					jtj[19] += row.J[4] * row.J[5];

					jtj[20] += row.J[5] * row.J[5];

					// extra info here
					info[0] += 1;
				}
			}

			for(int i = 0; i < 32; ++i) { // copy over to shared memory
				S[sline][i] = sums[i];
			}
			// WE NO LONGER NEED TO DO THIS AS the threads execute sequentially inside a for loop

		} // threads now execute as a for loop.
		  //so the __syncthreads() is irrelevant

		for(int ssline = 0; ssline < 32; ssline++) { // sum up columns and copy to global memory in the final 32 threads
			for(unsigned i = 1; i < 112 /*blockDim.x*/; ++i) {
				S[0][ssline] += S[i][ssline];
			}
			out[ssline+blockIndex*32] = S[0][ssline];
		}
	#else 
		new_reduce(blockIndex, out, J, Jsize, size);
	#endif

	}

	TooN::Matrix<8, 32, float, TooN::Reference::RowMajor> values(out);
	for (int j = 1; j < 8; ++j) {
		values[0] += values[j];
	}
	TOCK("reduceKernel", 512);
}

void trackKernel(TrackData* output, const float3* inVertex,
		const float3* inNormal, uint2 inSize, const float3* refVertex,
		const float3* refNormal, uint2 refSize, const Matrix4 Ttrack,
		const Matrix4 view, const float dist_threshold,
		const float normal_threshold) {
	TICK();
	uint2 pixel = make_uint2(0, 0);
	unsigned int pixely, pixelx;
	#pragma omp parallel for \
	    shared(output), private(pixel,pixelx,pixely)
	for (pixely = 0; pixely < inSize.y; pixely++) {
		for (pixelx = 0; pixelx < inSize.x; pixelx++) {
			pixel.x = pixelx;
			pixel.y = pixely;

			TrackData & row = output[pixel.x + pixel.y * refSize.x];

			if (inNormal[pixel.x + pixel.y * inSize.x].x == KFUSION_INVALID) {
				row.result = -1;
				continue;
			}

			const float3 projectedVertex = Ttrack
					* inVertex[pixel.x + pixel.y * inSize.x];
			const float3 projectedPos = view * projectedVertex;
			const float2 projPixel = make_float2(
					projectedPos.x / projectedPos.z + 0.5f,
					projectedPos.y / projectedPos.z + 0.5f);
			if (projPixel.x < 0 || projPixel.x > refSize.x - 1
					|| projPixel.y < 0 || projPixel.y > refSize.y - 1) {
				row.result = -2;
				continue;
			}

			const uint2 refPixel = make_uint2(projPixel.x, projPixel.y);
			const float3 referenceNormal = refNormal[refPixel.x
					+ refPixel.y * refSize.x];

			if (referenceNormal.x == KFUSION_INVALID) {
				row.result = -3;
				continue;
			}

			const float3 diff = refVertex[refPixel.x + refPixel.y * refSize.x]
					- projectedVertex;
			const float3 projectedNormal = rotate(Ttrack,
					inNormal[pixel.x + pixel.y * inSize.x]);

			if (length(diff) > dist_threshold) {
				row.result = -4;
				continue;
			}
			if (dot(projectedNormal, referenceNormal) < normal_threshold) {
				row.result = -5;
				continue;
			}
			row.result = 1;
			row.error = dot(referenceNormal, diff);
			((float3 *) row.J)[0] = referenceNormal;
			((float3 *) row.J)[1] = cross(projectedVertex, referenceNormal);
		}
	}
	TOCK("trackKernel", inSize.x * inSize.y);
}

void mm2metersKernel(float * out, uint2 outSize, const ushort * in,
		uint2 inSize) {
	TICK();
	// Check for unsupported conditions
	if ((inSize.x < outSize.x) || (inSize.y < outSize.y)) {
		std::cerr << "Invalid ratio." << std::endl;
		exit(1);
	}
	if ((inSize.x % outSize.x != 0) || (inSize.y % outSize.y != 0)) {
		std::cerr << "Invalid ratio." << std::endl;
		exit(1);
	}
	if ((inSize.x / outSize.x != inSize.y / outSize.y)) {
		std::cerr << "Invalid ratio." << std::endl;
		exit(1);
	}

	int ratio = inSize.x / outSize.x;
	unsigned int y;
	#pragma omp parallel for \
        shared(out), private(y)
	for (y = 0; y < outSize.y; y++)
		for (unsigned int x = 0; x < outSize.x; x++) {
			out[x + outSize.x * y] = in[x * ratio + inSize.x * y * ratio]
					/ 1000.0f;
		}
	TOCK("mm2metersKernel", outSize.x * outSize.y);
}

void halfSampleRobustImageKernel(float* out, const float* in, uint2 inSize,
		const float e_d, const int r) {
	TICK();
	uint2 outSize = make_uint2(inSize.x / 2, inSize.y / 2);
	unsigned int y;
	#pragma omp parallel for \
        shared(out), private(y)
	for (y = 0; y < outSize.y; y++) {
		for (unsigned int x = 0; x < outSize.x; x++) {
			uint2 pixel = make_uint2(x, y);
			const uint2 centerPixel = 2 * pixel;

			float sum = 0.0f;
			float t = 0.0f;
			const float center = in[centerPixel.x
					+ centerPixel.y * inSize.x];
			for (int i = -r + 1; i <= r; ++i) {
				for (int j = -r + 1; j <= r; ++j) {
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
	TOCK("halfSampleRobustImageKernel", outSize.x * outSize.y);
}


void integrateKernel(Volume vol, float* depth, uint2 depthSize,
		Matrix4 invTrack, Matrix4 K, float mu,
		float maxweight) {

	TICK();
    int argcount = 0;
	int start = 0;
	int end = 256;

	pt_out[0] = volumeData_BUF;

	pt_in[0] = volumeSize_BUF;
	pt_in[1] = volumeData_BUF;
	pt_in[2] = volumeDim_BUF;
	pt_in[3] = floatDepth_BUF;
	pt_in[4] = inv_Ttrack_BUF;
	pt_in[5] = cameraK_BUF;

	// before migration
	volumeSize->x = volume.size.x;
	volumeSize->y = volume.size.y;
	volumeSize->z = volume.size.z;

	volumeDim->x = volume.dim.x;
	volumeDim->y = volume.dim.y;
	volumeDim->z = volume.dim.z;

	#pragma omp parallel for
	for (int i = 0; i < 4; i ++) {
		inv_Ttrack[i*4] = invTrack.data[i].x;
		inv_Ttrack[i*4 + 1] = invTrack.data[i].y;
		inv_Ttrack[i*4 + 2] = invTrack.data[i].z;
		inv_Ttrack[i*4 + 3] = invTrack.data[i].w;

		cameraK[i*4] = K.data[i].x;
		cameraK[i*4 + 1] = K.data[i].y;
		cameraK[i*4 + 2] = K.data[i].z;
		cameraK[i*4 + 3] = K.data[i].w;
	}


  	
	OCL_CHECK(err, err = clSetKernelArg(K_integrateKernel,	argcount++, sizeof(cl_mem), &volumeSize_BUF));
	OCL_CHECK(err, err = clSetKernelArg(K_integrateKernel,	argcount++, sizeof(cl_mem), &volumeData_BUF));
	OCL_CHECK(err, err = clSetKernelArg(K_integrateKernel,	argcount++, sizeof(cl_mem), &volumeData_BUF));
	OCL_CHECK(err, err = clSetKernelArg(K_integrateKernel,	argcount++, sizeof(cl_mem), &volumeDim_BUF));
	OCL_CHECK(err, err = clSetKernelArg(K_integrateKernel,	argcount++, sizeof(cl_mem), &floatDepth_BUF));
	OCL_CHECK(err, err = clSetKernelArg(K_integrateKernel,	argcount++, sizeof(int), &depthSize.x));
	OCL_CHECK(err, err = clSetKernelArg(K_integrateKernel,	argcount++, sizeof(int), &depthSize.y));
	OCL_CHECK(err, err = clSetKernelArg(K_integrateKernel,	argcount++, sizeof(cl_mem), &inv_Ttrack_BUF));
	OCL_CHECK(err, err = clSetKernelArg(K_integrateKernel,	argcount++, sizeof(cl_mem), &cameraK_BUF));
	OCL_CHECK(err, err = clSetKernelArg(K_integrateKernel,	argcount++, sizeof(float), &mu));
	OCL_CHECK(err, err = clSetKernelArg(K_integrateKernel,	argcount++, sizeof(float), &maxweight));
	OCL_CHECK(err, err = clSetKernelArg(K_integrateKernel,	argcount++, sizeof(int), &start));
	OCL_CHECK(err, err = clSetKernelArg(K_integrateKernel,	argcount++, sizeof(int), &end));
	
	OCL_CHECK(err, err = clEnqueueMigrateMemObjects(Q_integrateKernel,(cl_uint)6, pt_in, 0 ,0,NULL, NULL));
	OCL_CHECK(err, err = clEnqueueTask(Q_integrateKernel, K_integrateKernel, 0, NULL, NULL));
	OCL_CHECK(err, err = clEnqueueMigrateMemObjects(Q_integrateKernel,(cl_uint)1, pt_out, CL_MIGRATE_MEM_OBJECT_HOST,0,NULL, NULL));

	clFinish(Q_integrateKernel);
	
	TOCK("integrateKernel", vol.size.x * vol.size.y);
}


float4 raycast(const Volume volume, const uint2 pos, const Matrix4 view,
		const float nearPlane, const float farPlane, const float step,
		const float largestep) {

	const float3 origin = get_translation(view);
	const float3 direction = rotate(view, make_float3(pos.x, pos.y, 1.f));

	// intersect ray with a box
	// http://www.siggraph.org/education/materials/HyperGraph/raytrace/rtinter3.htm
	// compute intersection of ray with all six bbox planes
	const float3 invR = make_float3(1.0f) / direction;
	const float3 tbot = -1 * invR * origin;
	const float3 ttop = invR * (volume.dim - origin);

	// re-order intersections to find smallest and largest on each axis
	const float3 tmin = fminf(ttop, tbot);
	const float3 tmax = fmaxf(ttop, tbot);

	// find the largest tmin and the smallest tmax
	const float largest_tmin = fmaxf(fmaxf(tmin.x, tmin.y),
			fmaxf(tmin.x, tmin.z));
	const float smallest_tmax = fminf(fminf(tmax.x, tmax.y),
			fminf(tmax.x, tmax.z));

	// check against near and far plane
	const float tnear = fmaxf(largest_tmin, nearPlane);
	const float tfar = fminf(smallest_tmax, farPlane);

	if (tnear < tfar) {
		// first walk with largesteps until we found a hit
		float t = tnear;
		float stepsize = largestep;
		float f_t = volume.interp(origin + direction * t);
		float f_tt = 0;
		if (f_t > 0) { // ups, if we were already in it, then don't render anything here
			for (; t < tfar; t += stepsize) {
				f_tt = volume.interp(origin + direction * t);
				if (f_tt < 0)                  // got it, jump out of inner loop
					break;
				if (f_tt < 0.8f)               // coming closer, reduce stepsize
					stepsize = step;
				f_t = f_tt;
			}
			if (f_tt < 0) {           // got it, calculate accurate intersection
				t = t + stepsize * f_tt / (f_t - f_tt);
				return make_float4(origin + direction * t, t);
			}
		}
	}
	return make_float4(0);

}

void raycastKernel(float4* vertex, float4* normal, uint2 inputSize,
		const Volume integration, const Matrix4 view, const float nearPlane,
		const float farPlane, const float step, const float largestep) {
	TICK();
	unsigned int y;
	#pragma omp parallel for \
	    shared(normal, vertex), private(y)
	for (y = 0; y < inputSize.y; y++)
		for (unsigned int x = 0; x < inputSize.x; x++) {

			uint2 pos = make_uint2(x, y);

			const float4 hit = raycast(integration, pos, view, nearPlane,
					farPlane, step, largestep);
			if (hit.w > 0.0) {
				vertex[pos.x + pos.y * inputSize.x] = hit;
				float3 surfNorm = integration.grad(make_float3(hit));
				if (length(surfNorm) == 0) {
					//normal[pos] = normalize(surfNorm); // APN added
					normal[pos.x + pos.y * inputSize.x].x = KFUSION_INVALID;
				} else {
					normal[pos.x + pos.y * inputSize.x] = make_float4(normalize(surfNorm));
				}
			} else {
				//std::cerr<< "RAYCAST MISS "<<  pos.x << " " << pos.y <<"  " << hit.w <<"\n";
				vertex[pos.x + pos.y * inputSize.x] = make_float4(0);
				normal[pos.x + pos.y * inputSize.x] = make_float4(KFUSION_INVALID, 0,
						0,0);
			}
		}
	TOCK("raycastKernel", inputSize.x * inputSize.y);
}


bool updatePoseKernel(Matrix4 & pose, const float * output,
		float icp_threshold) {
	bool res = false;
	TICK();
	// Update the pose regarding the tracking result
	TooN::Matrix<8, 32, const float, TooN::Reference::RowMajor> values(output);
	TooN::Vector<6> x = solve(values[0].slice<1, 27>());
	TooN::SE3<> delta(x);
	pose = toMatrix4(delta) * pose;

	// Return validity test result of the tracking
	if (norm(x) < icp_threshold)
		res = true;

	TOCK("updatePoseKernel", 1);
	return res;
}

bool checkPoseKernel(Matrix4 & pose, Matrix4 oldPose, const float * output,
		uint2 imageSize, float track_threshold) {

	// Check the tracking result, and go back to the previous camera position if necessary

	TooN::Matrix<8, 32, const float, TooN::Reference::RowMajor> values(output);

	if ((std::sqrt(values(0, 0) / values(0, 28)) > 2e-2)
			|| (values(0, 28) / (imageSize.x * imageSize.y) < track_threshold)) {
		pose = oldPose;
		return false;
	} else {
		return true;
	}

}

void renderNormalKernel(uchar3* out, const float3* normal, uint2 normalSize) {
	TICK();
	unsigned int y;
	#pragma omp parallel for \
        shared(out), private(y)
	for (y = 0; y < normalSize.y; y++)
		for (unsigned int x = 0; x < normalSize.x; x++) {
			uint pos = (x + y * normalSize.x);
			float3 n = normal[pos];
			if (n.x == -2) {
				out[pos] = make_uchar3(0, 0, 0);
			} else {
				n = normalize(n);
				out[pos] = make_uchar3(n.x * 128 + 128, n.y * 128 + 128,
						n.z * 128 + 128);
			}
		}
	TOCK("renderNormalKernel", normalSize.x * normalSize.y);
}

void renderDepthKernel(uchar4* out, float * depth, uint2 depthSize,
		const float nearPlane, const float farPlane) {
	TICK();

	float rangeScale = 1 / (farPlane - nearPlane);

	unsigned int y;
	#pragma omp parallel for \
        shared(out), private(y)
	for (y = 0; y < depthSize.y; y++) {
		int rowOffeset = y * depthSize.x;
		for (unsigned int x = 0; x < depthSize.x; x++) {

			unsigned int pos = rowOffeset + x;

			if (depth[pos] < nearPlane)
				out[pos] = make_uchar4(255, 255, 255, 0); // The forth value is a padding in order to align memory
			else {
				if (depth[pos] > farPlane)
					out[pos] = make_uchar4(0, 0, 0, 0); // The forth value is a padding in order to align memory
				else {
					const float d = (depth[pos] - nearPlane) * rangeScale;
					out[pos] = gs2rgb(d);
				}
			}
		}
	}
	TOCK("renderDepthKernel", depthSize.x * depthSize.y);
}

void renderTrackKernel(uchar4* out, const TrackData* data, uint2 outSize) {
	TICK();

	unsigned int y;
	#pragma omp parallel for \
        shared(out), private(y)
	for (y = 0; y < outSize.y; y++)
		for (unsigned int x = 0; x < outSize.x; x++) {
			uint pos = x + y * outSize.x;
			switch (data[pos].result) {
			case 1:
				out[pos] = make_uchar4(128, 128, 128, 0);  // ok	 GREY
				break;
			case -1:
				out[pos] = make_uchar4(0, 0, 0, 0);      // no input BLACK
				break;
			case -2:
				out[pos] = make_uchar4(255, 0, 0, 0);        // not in image RED
				break;
			case -3:
				out[pos] = make_uchar4(0, 255, 0, 0);    // no correspondence GREEN
				break;
			case -4:
				out[pos] = make_uchar4(0, 0, 255, 0);        // to far away BLUE
				break;
			case -5:
				out[pos] = make_uchar4(255, 255, 0, 0);     // wrong normal YELLOW
				break;
			default:
				out[pos] = make_uchar4(255, 128, 128, 0);
				break;
			}
		}
	TOCK("renderTrackKernel", outSize.x * outSize.y);
}

void renderVolumeKernel(uchar4* out, const uint2 depthSize, const Volume volume,
		const Matrix4 view, const float nearPlane, const float farPlane,
		const float step, const float largestep, const float3 light,
		const float3 ambient) {
	TICK();
	unsigned int y;
	#pragma omp parallel for \
        shared(out), private(y)
	for (y = 0; y < depthSize.y; y++) {
		for (unsigned int x = 0; x < depthSize.x; x++) {
			const uint pos = x + y * depthSize.x;

			float4 hit = raycast(volume, make_uint2(x, y), view, nearPlane,
					farPlane, step, largestep);
			if (hit.w > 0) {
				const float3 test = make_float3(hit);
				const float3 surfNorm = volume.grad(test);
				if (length(surfNorm) > 0) {
					const float3 diff = normalize(light - test);
					const float dir = fmaxf(dot(normalize(surfNorm), diff),
							0.f);
					const float3 col = clamp(make_float3(dir) + ambient, 0.f,
							1.f) * 255;
					out[pos] = make_uchar4(col.x, col.y, col.z, 0); // The forth value is a padding to align memory
				} else {
					out[pos] = make_uchar4(0, 0, 0, 0); // The forth value is a padding to align memory
				}
			} else {
				out[pos] = make_uchar4(0, 0, 0, 0); // The forth value is a padding to align memory
			}
		}
	}
	TOCK("renderVolumeKernel", depthSize.x * depthSize.y);
}

bool Kfusion::preprocessing(const ushort * inputDepth, const uint2 inputSize) {

	mm2metersKernel(floatDepth, computationSize, inputDepth, inputSize);

	bilateralFilterKernel(scaledDepth[0], floatDepth, computationSize.x, computationSize.y, gaussian,
			e_delta, radius);

	return true;
}

bool Kfusion::tracking(float4 k, float icp_threshold, uint tracking_rate,
		uint frame) {

	if (frame % tracking_rate != 0)
		return false;

		TICK();
		int argcount = 0;
		float e_d = e_delta * 3;
		int r = 1;
		cameraK[0] = k.x; cameraK[1] = k.y; cameraK[2] = k.z; cameraK[3] = k.w;
		cl_mem pt_halfvertex_in[2] = {scaledDepth0_BUF, cameraK_BUF};
		cl_mem pt_halfvertex_out[3] = {inputVertex_BUF[0], inputVertex_BUF[1], inputVertex_BUF[2]};
		OCL_CHECK(err, err = clSetKernelArg(K_halfSampleDepth2vertexKernel,argcount++, sizeof(cl_mem), &inputVertex_BUF[0]));
		OCL_CHECK(err, err = clSetKernelArg(K_halfSampleDepth2vertexKernel,argcount++, sizeof(cl_mem), &inputVertex_BUF[1]));
		OCL_CHECK(err, err = clSetKernelArg(K_halfSampleDepth2vertexKernel,argcount++, sizeof(cl_mem), &inputVertex_BUF[2]));
		OCL_CHECK(err, err = clSetKernelArg(K_halfSampleDepth2vertexKernel,argcount++, sizeof(cl_mem), &scaledDepth0_BUF));
		OCL_CHECK(err, err = clSetKernelArg(K_halfSampleDepth2vertexKernel,argcount++, sizeof(uint), &computationSize.x));
		OCL_CHECK(err, err = clSetKernelArg(K_halfSampleDepth2vertexKernel,argcount++, sizeof(uint), &computationSize.y));
		OCL_CHECK(err, err = clSetKernelArg(K_halfSampleDepth2vertexKernel,argcount++, sizeof(float), &e_d));
		OCL_CHECK(err, err = clSetKernelArg(K_halfSampleDepth2vertexKernel,argcount++, sizeof(int), &r));
		OCL_CHECK(err, err = clSetKernelArg(K_halfSampleDepth2vertexKernel,argcount++, sizeof(cl_mem), &cameraK_BUF));
		
		
		OCL_CHECK(err, err = clEnqueueMigrateMemObjects(Q_halfSampleDepth2vertexKernel,(cl_uint)2, pt_halfvertex_in, 0 ,0,NULL, NULL));
		OCL_CHECK(err, err = clEnqueueTask(Q_halfSampleDepth2vertexKernel, K_halfSampleDepth2vertexKernel, 0, NULL, NULL));
		OCL_CHECK(err, err = clEnqueueMigrateMemObjects(Q_halfSampleDepth2vertexKernel,(cl_uint)3, pt_halfvertex_out, CL_MIGRATE_MEM_OBJECT_HOST,0,NULL, NULL));

		clFinish(Q_halfSampleDepth2vertexKernel);
		TOCK("halfSampleDepth2vertexKernel", computationSize.x*computationSize.y);

		TICK();
		for (unsigned int i = 0; i < iterations.size(); ++i) {
			int argcount = 0;
			uint2 localimageSize = make_uint2(computationSize.x/(int)pow(2, i), computationSize.y/(int)pow(2, i));
			OCL_CHECK(err, err = clSetKernelArg(K_vertex2normalKernel,argcount++, sizeof(cl_mem), &inputNormal_BUF[i]));
			OCL_CHECK(err, err = clSetKernelArg(K_vertex2normalKernel,argcount++, sizeof(cl_mem), &inputVertex_BUF[i]));
			OCL_CHECK(err, err = clSetKernelArg(K_vertex2normalKernel,argcount++, sizeof(uint), &localimageSize.x));
			OCL_CHECK(err, err = clSetKernelArg(K_vertex2normalKernel,argcount++, sizeof(uint), &localimageSize.y));
			
			OCL_CHECK(err, err = clEnqueueMigrateMemObjects(Q_vertex2normalKernel,(cl_uint)1, &inputVertex_BUF[i], 0 ,0,NULL, NULL));
			OCL_CHECK(err, err = clEnqueueTask(Q_vertex2normalKernel, K_vertex2normalKernel, 0, NULL, NULL));
			OCL_CHECK(err, err = clEnqueueMigrateMemObjects(Q_vertex2normalKernel,(cl_uint)1, &inputNormal_BUF[i], CL_MIGRATE_MEM_OBJECT_HOST,0,NULL, NULL));

			clFinish(Q_vertex2normalKernel);
		}
		TICK("vertex2normalKernel", computationSize.x*computationSize.y);
	
	oldPose = pose;
	const Matrix4 projectReference = getCameraMatrix(k) * inverse(raycastPose);

	for (int level = iterations.size() - 1; level >= 0; --level) {
		uint2 localimagesize = make_uint2(
				computationSize.x / (int) pow(2, level),
				computationSize.y / (int) pow(2, level));
		for (int i = 0; i < iterations[level]; ++i) {

			
			pt_tr_in[0] = inputVertex_BUF[level];
			pt_tr_in[1] = inputNormal_BUF[level];
			pt_tr_in[2] = vertex_BUF;
			pt_tr_in[3] = normal_BUF;
			pt_tr_in[4] = Ttrack_BUF;
			pt_tr_in[5] = view_BUF;

			pt_tr_out[0] = reductionoutput_BUF;
			// pt_tr_out[0] = trackingResult_BUF;
			// pt_tr_out[0] = trackingResult_data_BUF;
			// pt_tr_out[1] = trackingResult_float_BUF;


			TICK();

			int k;
			#pragma omp parallel for
			for (k = 0; k < 4; k ++) {
				Ttrack[k*4] = pose.data[k].x;
				Ttrack[k*4 + 1] = pose.data[k].y;
				Ttrack[k*4 + 2] = pose.data[k].z;
				Ttrack[k*4 + 3] = pose.data[k].w;

				view[k*4] = projectReference.data[k].x;
				view[k*4 + 1] = projectReference.data[k].y;
				view[k*4 + 2] = projectReference.data[k].z;
				view[k*4 + 3] = projectReference.data[k].w;
			}

			int argcount = 0;

		
			OCL_CHECK(err, err = clSetKernelArg(K_trackANDreduceKernel,argcount++, sizeof(cl_mem), &reductionoutput_BUF));
			// OCL_CHECK(err, err = clSetKernelArg(K_trackANDreduceKernel,argcount++, sizeof(cl_mem), &trackingResult_float_BUF));
			OCL_CHECK(err, err = clSetKernelArg(K_trackANDreduceKernel,argcount++, sizeof(cl_mem), &inputVertex_BUF[level]));
			OCL_CHECK(err, err = clSetKernelArg(K_trackANDreduceKernel,argcount++, sizeof(cl_mem), &inputNormal_BUF[level]));
			OCL_CHECK(err, err = clSetKernelArg(K_trackANDreduceKernel,argcount++, sizeof(int), &localimagesize.x));
			OCL_CHECK(err, err = clSetKernelArg(K_trackANDreduceKernel,argcount++, sizeof(int), &localimagesize.y));
			OCL_CHECK(err, err = clSetKernelArg(K_trackANDreduceKernel,argcount++, sizeof(cl_mem), &vertex_BUF));
			OCL_CHECK(err, err = clSetKernelArg(K_trackANDreduceKernel,argcount++, sizeof(cl_mem), &normal_BUF));
			OCL_CHECK(err, err = clSetKernelArg(K_trackANDreduceKernel,argcount++, sizeof(int), &computationSize.x));
			OCL_CHECK(err, err = clSetKernelArg(K_trackANDreduceKernel,argcount++, sizeof(int), &computationSize.y));
			OCL_CHECK(err, err = clSetKernelArg(K_trackANDreduceKernel,argcount++, sizeof(cl_mem), &Ttrack_BUF));
			OCL_CHECK(err, err = clSetKernelArg(K_trackANDreduceKernel,argcount++, sizeof(cl_mem), &view_BUF));
			OCL_CHECK(err, err = clSetKernelArg(K_trackANDreduceKernel,argcount++, sizeof(float), &dist_threshold));
			OCL_CHECK(err, err = clSetKernelArg(K_trackANDreduceKernel,argcount++, sizeof(float), &normal_threshold));
			
			OCL_CHECK(err, err = clEnqueueMigrateMemObjects(Q_trackANDreduceKernel,(cl_uint)6, pt_tr_in, 0 ,0,NULL, NULL));
			OCL_CHECK(err, err = clEnqueueTask(Q_trackANDreduceKernel, K_trackANDreduceKernel, 0, NULL, NULL));
			OCL_CHECK(err, err = clEnqueueMigrateMemObjects(Q_trackANDreduceKernel,(cl_uint)1, pt_tr_out, CL_MIGRATE_MEM_OBJECT_HOST,0,NULL, NULL));

			clFinish(Q_trackANDreduceKernel);

	TooN::Matrix<8, 32, float, TooN::Reference::RowMajor> values(reductionoutput);
	for (int j = 1; j < 8; ++j) {
		values[0] += values[j];
	}


			TOCK("trackAndreduceKernel", localimagesize.x * localimagesize.y);

			if (updatePoseKernel(pose, reductionoutput, icp_threshold))
				break;

		}
	}
	return checkPoseKernel(pose, oldPose, reductionoutput, computationSize,
			track_threshold);

}

bool Kfusion::raycasting(float4 k, float mu, uint frame) {

	bool doRaycast = false;

	if (frame > 2) {
		raycastPose = pose;
		raycastKernel(vertex, normal, computationSize, volume,
				raycastPose * getInverseCameraMatrix(k), nearPlane, farPlane,
				step, 0.75f * mu);
	}

	return doRaycast;

}

bool Kfusion::integration(float4 k, uint integration_rate, float mu,
		uint frame) {


	bool doIntegrate = checkPoseKernel(pose, oldPose, reductionoutput,
			computationSize, track_threshold);

	if ((doIntegrate && ((frame % integration_rate) == 0)) || (frame <= 3)) {
		integrateKernel(volume, floatDepth, computationSize, inverse(pose),
				getCameraMatrix(k), mu, maxweight);
		doIntegrate = true;
	} else {
		doIntegrate = false;
	}


	return doIntegrate;

}

void Kfusion::dumpVolume(const char *filename) {

	std::ofstream fDumpFile;
	std::ofstream fDumpTextFile;
	std::ofstream fDumpPlyFile;
	std::string textfilename(filename);
	std::string plyfilename(filename);
	textfilename = textfilename + ".txt";
	plyfilename = plyfilename + ".ply";
	if (filename == NULL) {
		return;
	}

	std::cout << "Dumping the volumetric representation on file: " << filename
			<< std::endl;
	fDumpFile.open(filename, std::ios::out | std::ios::binary);
	fDumpTextFile.open(textfilename.c_str(), std::ios::out);
	fDumpPlyFile.open(plyfilename.c_str(), std::ios::out | std::ios::binary);
	if (fDumpFile.fail()) {
		std::cout << "Error opening file: " << filename << std::endl;
		exit(1);
	}
// (x, y, z, intensity) txt file
	for(int x=0; x<256; x++){
		for(int y=0; y<256; y++){
			for(int z=0; z<256; z++){
				short i = (volume.data)[x*256*256 + y*256 + z].x;
				fDumpTextFile << x << "\t"
								<< y << "\t"
								<< z << "\t"
								<< i<< "\n";

			}
		}
	}
	fDumpTextFile.close();
// (x, y, z, r, g, b) ply file
	int volumeSize = 256*256*256;
	int validVertex = 0;
	for(int x=0; x<volume.size.x; x++){
		for(int y=0; y<volume.size.y; y++){
			for(int z=0; z<volume.size.z; z++){
				if(volume.data[x*256*256 + y*256 + z].x==32766) validVertex++; 
			}
		}
	}

	std::cout << "valid vertex: " << validVertex << "\n";
	const char header1[] = "ply\r\nformat binary_little_endian 1.0\r\ncomment Binary to Ply converted\r\nelement vertex ";
	char num[10];
	sprintf(num, "%d", validVertex);
	const char header2[] = "\r\nproperty short x\r\nproperty short y\r\nproperty short z\r\nproperty uchar red\r\nproperty uchar green\r\nproperty uchar blue\r\nend_header\r\n";
	fDumpPlyFile.write(header1, strlen(header1));
	fDumpPlyFile.write(num, strlen(num));
	fDumpPlyFile.write(header2, strlen(header2));
	
	for(int x=0; x<volume.size.x; x++){
		for(int y=0; y<volume.size.y; y++){
			for(int z=0; z<volume.size.z; z++){
				int i = x*256*256 + y*256 + z;
            	int reverse_i = 16777215 - i;
				if(volume.data[reverse_i].x==32766) continue; 
				short x_16 = (short)x;
				short y_16 = (short)y;
				short z_16 = (short)z;
				char rgb = (char)(volume.data[reverse_i].x >> 8);
				fDumpPlyFile.write((char*)(&x_16), sizeof(short));
				fDumpPlyFile.write((char*)(&y_16), sizeof(short));
				fDumpPlyFile.write((char*)(&z_16), sizeof(short));
				fDumpPlyFile.write((char*)(&rgb), sizeof(char));
				fDumpPlyFile.write((char*)(&rgb), sizeof(char));
				fDumpPlyFile.write((char*)(&rgb), sizeof(char));
			}
		}
	}

	fDumpPlyFile.close();



	// Dump on file without the y component of the short2 variable
	for (unsigned int i = 0; i < volume.size.x * volume.size.y * volume.size.z;
			i++) {
		fDumpFile.write((char *) (volume.data + i), sizeof(short));
	}

	fDumpFile.close();

}

void Kfusion::renderVolume(uchar4 * out, uint2 outputSize, int frame,
		int raycast_rendering_rate, float4 k, float largestep) {
	if (frame % raycast_rendering_rate == 0)
		renderVolumeKernel(out, outputSize, volume,
				*(this->viewPose) * getInverseCameraMatrix(k), nearPlane,
				farPlane * 2.0f, step, largestep, light, ambient);
}

void Kfusion::renderTrack(uchar4 * out, uint2 outputSize) {
	// renderTrackKernel(out, trackingResult, outputSize);
}

void Kfusion::renderDepth(uchar4 * out, uint2 outputSize) {
	renderDepthKernel(out, floatDepth, outputSize, nearPlane, farPlane);
}

void Kfusion::computeFrame(const ushort * inputDepth, const uint2 inputSize,
			 float4 k, uint integration_rate, uint tracking_rate,
			 float icp_threshold, float mu, const uint frame) {
  preprocessing(inputDepth, inputSize);
  _tracked = tracking(k, icp_threshold, tracking_rate, frame);
  _integrated = integration(k, integration_rate, mu, frame);
  raycasting(k, mu, frame);
}


void synchroniseDevices() {
	// Nothing to do in the C++ implementation
}
