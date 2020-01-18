/**
 * grep.cpp
 * Implementation of grep library using system call.
 */

#include "grep.h"

string grep(const string &input) {
    /// Check whether this is a grep command.
    istringstream iss(input);
    string firstWord;
    iss >> firstWord;
    if (firstWord != "grep")
        throw runtime_error("Not a grep command\n");

    /// Run grep command.
    ostringstream command;
    command << input << " > result";
    if (system(command.str().c_str()) != 0)
        throw runtime_error("Bad command\n");

    /// Convert output to string and return.
    ifstream res("result");
    string str((istreambuf_iterator<char>(res)), istreambuf_iterator<char>());

    return str;
}
