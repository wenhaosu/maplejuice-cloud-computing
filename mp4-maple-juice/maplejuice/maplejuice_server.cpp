#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <map>

using namespace std;

//files/local/englishText_0_10000

int main() {
    system("./wc < files/local/englishText_0_10000 >result.txt");
    map<string, int> mp;
    string temp;
    ifstream res("result.txt");
    while (getline(res, temp, '\n')) {
        string key;
        int val = 0;
        stringstream ss(temp);
        ss >> key >> val;
        mp[key] += val;
    }
    for (const auto& item : mp) {
        cout << item.first << " " << item.second << endl;
    }
    return 0;
}
