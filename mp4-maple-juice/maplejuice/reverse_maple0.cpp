/**
 * reverse_maple0.cpp
 * Maple 0 phase for wordcount.
 */

#include <iostream>
#include <sstream>

using namespace std;

int main() {
    ios_base::sync_with_stdio(false);
    string input;
    while (getline(cin, input)) {
        string val1, val2;
        stringstream ss(input);
        ss >> val1 >> val2;

        cout << val2 << "\t" << val1 << "\n";
    }
    return 0;
}
