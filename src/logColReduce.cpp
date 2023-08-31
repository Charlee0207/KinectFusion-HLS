#include <iostream>
#include <fstream>
#include <string>

int main(int argc, char ** argv){
    if(argc !=2)
        std::cerr << "Incorrect argument numbers!\n";
    std::string input(argv[1]);
    std::string output;
    output = input.substr(0, input.size()-4);
    output += "ColReduced.log";
    std::ifstream fin(input.c_str(), std::ios::in);
    std::ofstream fout(output.c_str(), std::ios::out);
    std::string elem[16];

    if(!fin)
        std::cerr << "Empty input file!\n";
    if(!fout)
        std::cerr << "Empty output file!\n";

    std::cout << "Reducing...\n";
    for(int row=0; row<881; row++){
        for(int col=0; col<16; col++){
            fin >> elem[col];
        }
        for(int col=0; col<14; col++){
            fout << elem[col] << (col==13 ? "\n" : "\t");
        }
    }

    std::cout << "Reduced!\n";
    std::cout << "Output log file to " << output << "\n";

    fin.close();
    fout.close();
}
