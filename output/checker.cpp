#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace std;

int main(){
    cout << "check scaledDepth enter \"bf\"\n";
    cout << "check trackresult enter \"tr\"\n";
    cout << "check reductionoutput enter \"rd\"\n";
    string sel;
    cin >> sel;
    if(sel=="bf"){
        string implFolder = "/home/jason/KinectFusion_U50/KinectFusion_HW_impl/output/HW_impl/";
        string baselineFolder = "/home/jason/KinectFusion_U50/KinectFusion_HW_baseline/output/HW_baseline/";
        string name;
        float implResult[240][320];
        float baselineResult[240][320];
        for(int iteration=0; iteration<882; iteration++){
            std::cout << iteration << "\n";
            name = implFolder + "scaledDepth_" + std::to_string(iteration); std::ifstream fimplResult(name, std::ios::in);
            name = baselineFolder + "scaledDepth_" + std::to_string(iteration); std::ifstream fbaselineResult(name, std::ios::in);
            if(fimplResult.fail()) return 0;
            if(fbaselineResult.fail()) return 0;
            for(int y=0; y<240; y++){
                for(int x=0; x<320; x++){
                    fimplResult >> implResult[y][x];
                    fbaselineResult >> baselineResult[y][x];
                }
            }
            
            fimplResult.close();
            fbaselineResult.close();
            for(int y=0; y<240; y++){
                for(int x=0; x<320; x++){
                    if(baselineResult[y][x]!=implResult[y][x]){
                        std::cout   << "HW_baseline/scaledDepth_" << iteration << ": " << baselineResult[y][x] << ", "
                                << "HW_impl/scaledDepth_" << iteration << ": " << implResult[y][x] << "\n";
                    }
                }
            }
        }
    }
    if(sel=="tr"){
        string implFolder = "/home/jason/KinectFusion_U50/KinectFusion_HW_impl/output/HW_impl/";
        string baselineFolder = "/home/jason/KinectFusion_U50/KinectFusion_HW_baseline/output/HW_baseline/";
        string name;
        float implResult[240][320];
        float implError[240][320];
        float implJ0[240][320];
        float implJ1[240][320];
        float implJ2[240][320];
        float implJ3[240][320];
        float implJ4[240][320];
        float implJ5[240][320];
        float baselineResult[240][320];
        float baselineError[240][320];
        float baselineJ0[240][320];
        float baselineJ1[240][320];
        float baselineJ2[240][320];
        float baselineJ3[240][320];
        float baselineJ4[240][320];
        float baselineJ5[240][320];
        for(int iteration=0; iteration<2300; iteration++){
            std::cout << iteration << "\n";
            name = implFolder + "result_" + std::to_string(iteration); std::ifstream fimplResult(name, std::ios::in);
            name = implFolder + "error_" + std::to_string(iteration); std::ifstream fimplError(name, std::ios::in);
            name = implFolder + "J0_" + std::to_string(iteration); std::ifstream fimplJ0(name, std::ios::in);
            name = implFolder + "J1_" + std::to_string(iteration); std::ifstream fimplJ1(name, std::ios::in);
            name = implFolder + "J2_" + std::to_string(iteration); std::ifstream fimplJ2(name, std::ios::in);
            name = implFolder + "J3_" + std::to_string(iteration); std::ifstream fimplJ3(name, std::ios::in);
            name = implFolder + "J4_" + std::to_string(iteration); std::ifstream fimplJ4(name, std::ios::in);
            name = implFolder + "J5_" + std::to_string(iteration); std::ifstream fimplJ5(name, std::ios::in);

            
            name = baselineFolder + "result_" + std::to_string(iteration); std::ifstream fbaselineResult(name, std::ios::in);
            name = baselineFolder + "error_" + std::to_string(iteration); std::ifstream fbaselineError(name, std::ios::in);
            name = baselineFolder + "J0_" + std::to_string(iteration); std::ifstream fbaselineJ0(name, std::ios::in);
            name = baselineFolder + "J1_" + std::to_string(iteration); std::ifstream fbaselineJ1(name, std::ios::in);
            name = baselineFolder + "J2_" + std::to_string(iteration); std::ifstream fbaselineJ2(name, std::ios::in);
            name = baselineFolder + "J3_" + std::to_string(iteration); std::ifstream fbaselineJ3(name, std::ios::in);
            name = baselineFolder + "J4_" + std::to_string(iteration); std::ifstream fbaselineJ4(name, std::ios::in);
            name = baselineFolder + "J5_" + std::to_string(iteration); std::ifstream fbaselineJ5(name, std::ios::in);

            if(fimplResult.fail()) return 0;
            if(fbaselineResult.fail()) return 0;
            for(int y=0; y<240; y++){
                for(int x=0; x<320; x++){
                    fimplResult >> implResult[y][x];
                    fimplError >> implError[y][x];
                    fimplJ0 >> implJ0[y][x];
                    fimplJ1 >> implJ1[y][x];
                    fimplJ2 >> implJ2[y][x];
                    fimplJ3 >> implJ3[y][x];
                    fimplJ4 >> implJ4[y][x];
                    fimplJ5 >> implJ5[y][x];

                    fbaselineResult >> baselineResult[y][x];
                    fbaselineError >> baselineError[y][x];
                    fbaselineJ0 >> baselineJ0[y][x];
                    fbaselineJ1 >> baselineJ1[y][x];
                    fbaselineJ2 >> baselineJ2[y][x];
                    fbaselineJ3 >> baselineJ3[y][x];
                    fbaselineJ4 >> baselineJ4[y][x];
                    fbaselineJ5 >> baselineJ5[y][x];
                }
            }
            
            fimplResult.close();
            fimplError.close();
            fimplJ0.close();
            fimplJ1.close();
            fimplJ2.close();
            fimplJ3.close();
            fimplJ4.close();
            fimplJ5.close();

            fbaselineResult.close();
            fbaselineError.close();
            fbaselineJ0.close();
            fbaselineJ1.close();
            fbaselineJ2.close();
            fbaselineJ3.close();
            fbaselineJ4.close();
            fbaselineJ5.close();
            for(int y=0; y<240; y++){
                for(int x=0; x<320; x++){
                    if(baselineResult[y][x]!=implResult[y][x]){
                        std::cout   << "HW_baseline/result_" << iteration << ": " << baselineResult[y][x] << ", "
                                << "HW_impl/result" << iteration << ": " << implResult[y][x] << "\n";
                    }
                    if(baselineError[y][x]!=implError[y][x]){
                        std::cout   << "HW_baseline/error_" << iteration << ": " << baselineError[y][x] << ", "
                                << "HW_impl/error_" << iteration << ": " << implError[y][x] << "\n";
                    }
                    if(baselineJ0[y][x]!=implJ0[y][x]){
                        std::cout   << "HW_baseline/J0_" << iteration << ": " << baselineJ0[y][x] << ", "
                                << "HW_impl/J0_" << iteration << ": " << implJ0[y][x] << "\n";
                    }
                    if(baselineJ1[y][x]!=implJ1[y][x]){
                        std::cout   << "HW_baseline/J1_" << iteration << ": " << baselineJ1[y][x] << ", "
                                << "HW_impl/J1_" << iteration << ": " << implJ1[y][x] << "\n";
                    }
                    if(baselineJ2[y][x]!=implJ2[y][x]){
                        std::cout   << "HW_baseline/J2_" << iteration << ": " << baselineJ2[y][x] << ", "
                                << "HW_impl/J2_" << iteration << ": " << implJ2[y][x] << "\n";
                    }
                    if(baselineJ3[y][x]!=implJ3[y][x]){
                        std::cout   << "HW_baseline/J3_" << iteration << ": " << baselineJ3[y][x] << ", "
                                << "HW_impl/J3_" << iteration << ": " << implJ3[y][x] << "\n";
                    }
                    if(baselineJ4[y][x]!=implJ4[y][x]){
                        std::cout   << "HW_baseline/J4_" << iteration << ": " << baselineJ4[y][x] << ", "
                                << "HW_impl/J4_" << iteration << ": " << implJ4[y][x] << "\n";
                    }
                    if(baselineJ5[y][x]!=implJ5[y][x]){
                        std::cout   << "HW_baseline/J5_" << iteration << ": " << baselineJ5[y][x] << ", "
                                << "HW_impl/J5_" << iteration << ": " << implJ5[y][x] << "\n";
                    }
                }
            }

        }
    }
    if(sel=="rd"){
        string implFolder = "/home/jason/KinectFusion_U50/KinectFusion_HW_impl/output/HW_impl/";
        string baselineFolder = "/home/jason/KinectFusion_U50/KinectFusion_HW_baseline/output/HW_baseline/";
        string name;
        float implResult[256];
        float baselineResult[256];
        for(int iteration=0; iteration<882; iteration++){
            std::cout << iteration << "\n";
            name = implFolder + "reduce_" + std::to_string(iteration); std::ifstream fimplResult(name, std::ios::in);
            name = baselineFolder + "reduce_" + std::to_string(iteration); std::ifstream fbaselineResult(name, std::ios::in);
            if(fimplResult.fail()) return 0;
            if(fbaselineResult.fail()) return 0;
            for(int y=0; y<8; y++){
                for(int x=0; x<32; x++){
                    int k = x+y*32;
                    fimplResult >> implResult[k];
                    fbaselineResult >> baselineResult[k];
                }
            }
            
            fimplResult.close();
            fbaselineResult.close();
            for(int y=0; y<8; y++){
                for(int x=0; x<32; x++){
                    int k = x+y*32;
                    if(baselineResult[k]-implResult[k]>1){
                        std::cout   << "HW_baseline/reductionoutput_" << iteration << ": " << baselineResult[k] << ", "
                                << "HW_impl/reductionoutput_" << iteration << ": " << implResult[k] << "\n";
                    }
                }
            }
        }
    }
}