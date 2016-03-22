#include<iostream>
#include<fstream>
#include<string>

using namespace std;

int main() {
    string line;
    bool storeline = true;
    ifstream myfile("../feature_output_lstm/omnetpp.csv");
    ofstream outfile("../feature_output_lstm/omnetpp_fixed.csv");

    if (myfile && outfile) {
        while (getline( myfile, line )) {
            for (int i=line.length()-2; i>=0; i--) {
                if (line[i] == ' ') {
                    cout << "i = " << i << endl;
                    string bb_count_str = line.substr(i);
                    cout << line << " BB_count: " << bb_count_str << endl;

                    int bb_count = std::stoi(bb_count_str);

                    if (bb_count > 250) 
                        storeline = false;
                    else 
                        storeline = true;
                    break;
                }    
            }
            if (storeline)
                outfile << line << "\n";
        }
 
    myfile.close();
    outfile.close();
    }

    else cout << "Could not read file." << endl;
    
    return 0;

}

