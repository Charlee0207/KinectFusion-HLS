# Kinect Fusion: FPGA Implementation and Acceleration of a Dense SLAM Algorithm

Developed by Lee Cheng-Hao during the 2023 ITRI EOSL Summer Intern Program.

This repository contains the 
software (`SW build`), 
hardware baseline (`HW baseline build`), 
and hardware-optimized (`HW optimize build`) builds 
for the Kinect Fusion SLAM algorithm.

## Experient Evironment

* Intel(R) Xeon(R) E-2288G CPU @ 3.70GHz
* AMD Alveo™ U50 FPGA
* AMD Vitis 2023.1
* Ubuntu 22.04

## Directory Structure

```
${ThisRepository}
   |- SW_impl_gui
   |   |- host_src
   |   \- output
   |
   |- SW_impl
   |   |- host_src
   |   \- output
   |
   |- HW_baseline
   |   |- host_src
   |   |- kernel_src
   |   \- output
   |
   |- HW_optimization
   |   |- host_src
   |   |- kernel_src
   |   \- output
   |
   |- src
   |   |- living_room_traj2_loop (extra download needed) 
   |   |- livingRoom2.gt.freiburg
   |   |- logColReduce.cpp
   |   |- displayPointCloud.py
   |   \- checkPos.py
   |
   |- lib
   |   |- common
   |   |- include
   |   |- qt
   |   |- thirdparty
   \   \- TooN
     
```

## Set up Environment

### 1. AMD Vitis Suite
Install necessary dependencies before Vitis installation:
```
sudo apt install libtinfo5 libncurses5 -y
```
And follow the offical installation guide

### 2. Xilinx® Runtime (XRT) for U50

* Go to [Alveo U50 Package](https://www.xilinx.com/products/boards-and-kits/alveo/u50.html#gettingStarted) File Downloads
Download and install them in order
 `Xilinx Runtime`   `Deployment Target Platform`    `Development Target Platform `
 * Set up VITIS and XRT variables 
 ```
 source <Vitis_install_path>/Vitis/2023.1/settings64.sh
 source /opt/xilinx/xrt/setup.sh  
 ```
 
 ### 3. qt5
 ```
 sudo apt install -y qtcreator qtbase5-dev qt5-qmake cmake
 ```
 
 ### 4. Dateset
 ```
cd ./src
mkdir living_room_traj2_loop
cd living_room_traj2_loop
wget http://www.doc.ic.ac.uk/~ahanda/living_room_traj2_loop.tgz
tar -xzf living_room_traj2_loop.tgz
 ```
The configuration uses `.depth` format rather than `.png` for depth files. 
The `.png` format dataset can be accessed from TUM RGB-D Compatible PNGs at [ICL-NUIM](https://www.doc.ic.ac.uk/~ahanda/VaFRIC/iclnuim.html)
 
## Build Descriptions
### SW_impl_gui
 This build is modified from [slambench1](https://github.com/pamela-project/slambench1/tree/master), and extracted includes are present.
 
 Host Compile Flags:
 ```
 -I /usr/include/ -I /usr/local/include/ -I /usr/include/x86_64-linux-gnu/qt5/QtWidgets/ -I ./lib/include -I ./lib/qt -I ./lib/thirdparty
 ```
 ```
 -fopenmp
 ```
 ```
 -l gomp -l Qt5Core -l Qt5Widgets -l Qt5Gui -l Qt5OpenGL -l Qt5PrintSupport -l stdc++ -l GL -l GLU -l GLUT
 ```
 ```
 -L /usr/lib/x86_64-linux-gnu
 ```
 
Program Argument:
 ```
 -p 0.34,0.5,0.24 -c 2 -r 1 -t 1 -z 4 -l 0.00001 -m 0.1 -s 6.4 -v 256 -y 10,5,4 -i ./src/living_room_traj2_loop -d ./SW_impl_gui/output/output -o ./SW_impl_gui/output/log.log
 ```
### SW_impl
 This gui kernel is removed in this build, and the build runs without rendering but includes benchmarks.
 
 Host Compile Flags:
 ```
-I ./lib/common -I ./lib/include -I ./lib/thirdparty -I ./lib/TooN/include
 ```
 ```
 -fopenmp
 ```
 
Program Argument:
 ```
 -p 0.34,0.5,0.24 -c 2 -r 1 -t 1 -z 4 -l 0.00001 -m 0.1 -s 6.4 -v 256 -y 10,5,4 -i ./src/living_room_traj2_loop -d ./SW_impl/output/output -o ./SW_impl/output/log.log
 ```
 
### HW_baseline
 Host and Kernel Compile Flags:
 ```
-I ./lib/common -I ./lib/include -I ./lib/thirdparty -I ./lib/TooN/include
 ```
 ```
 -fopenmp
 ```
 
Program Argument:
 ```
 -p 0.34,0.5,0.24 -c 2 -r 1 -t 1 -z 4 -l 0.00001 -m 0.1 -s 6.4 -v 256 -y 10,5,4 -i ./src/living_room_traj2_loop -d ./HW_baseline/output/output -o ./HW_baseline/output/log.log
 ```
 
### HW_optimize
 Host and Kernel Compile Flags:
 ```
-I ./lib/common -I ./lib/include -I ./lib/thirdparty -I ./lib/TooN/include
 ```
 ```
 -fopenmp
 ```
 
Program Argument:
 ```
 -p 0.34,0.5,0.24 -c 2 -r 1 -t 1 -z 4 -l 0.00001 -m 0.1 -s 6.4 -v 256 -y 10,5,4 -i ./src/living_room_traj2_loop -d ./HW_optimize/output/output -o ./HW_optimize/output/log.log
 ```
 
## Result 
### View the 3D Reconstruction in voxel
```
./src/displayPointCloud.py ./<BUILD_NAME>/output/output.ply 
```

### Check the Camera Pose Error(ATE)
```
./src/checkPos.py ./<BUILD_NAME>/output/log.log  ./src/livingRoom2.gt.freiburg
```
If the number of column in `log.log` is mismatched, run:
```
g++ -o ./src/logColReduce ./src/logColReduce.cpp
./src/logColReduce ./<BUILD_NAME>/output/log.log
./src/checkPos.py ./<BUILD_NAME>/output/logColReduced.log ./src/livingRoom2.gt.freiburg
```

### Check the Kernel Time
```
./src/checkKernels.py ./kernel_timings.log
```

### The Program Demo in Runtime
https://github.com/Charlee0207/KinectFusion-HLS/assets/85032763/53e0ae62-65af-4d8a-bc4c-df7d66012c22


### The Reconstructed Scenario
https://github.com/Charlee0207/KinectFusion-HLS/assets/85032763/c129b0ce-56f3-4c77-bd58-e3ead15a39b8

### The Performace and Utilization
|![image](https://github.com/Charlee0207/KinectFusion-HLS/assets/85032763/bb398bef-76eb-414e-9ba4-3f1afd931af9)|![image](https://github.com/Charlee0207/KinectFusion-HLS/assets/85032763/b08e307b-e5bd-457e-9dd0-1e6a9d513115)|
|---|---|


## License

The code from [SLAMBench1](https://github.com/pamela-project/slambench1) and [Computer Systems Lab - University of Thessaly](https://github.com/csl-uth/KinectFusion-fpga/tree/main) repositories are used, which each one retains its original license. 
