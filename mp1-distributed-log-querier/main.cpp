/*
 * main.cpp
 *
 * Main function for server/client in MP0.
 */

#include "grep.h"

int main(int argc, char *argv[]) {

    string input;
    while (getline(cin, input)) {
        if (input == "exit") return 0;

        try {
            string res = grep(input);
            cout << res;
        }
        catch (string &err) { cerr << err << endl; }
    }

    return 0;
}
