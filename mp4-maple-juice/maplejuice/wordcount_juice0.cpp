/**
 * wordcount_juice0.cpp
 * Juice 0 phase for wordcount.
 */

#include <iostream>
#include <sstream>
#include <map>

using namespace std;

int main() {
    map<string, int> mp;
    ios_base::sync_with_stdio(false);
    string input;
    while (getline(cin, input)) {
        stringstream ss(input);
        string key;
        ss >> key;
        mp[key]++;
    }
    for (const auto &item : mp) {
        cout << item.first << "\t" << item.second << endl;
    }
    return 0;
}
