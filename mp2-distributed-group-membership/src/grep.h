/**
 * grep.h
 * Implementation of grep library using system call.
 */

#ifndef GREP_H
#define GREP_H

#include <iostream>
#include <iterator>
#include <fstream>
#include <cassert>
#include <sstream>
#include <stdexcept>

using namespace std;

/**
 * Run the given grep command using system call.
 *
 * Parameters:
 *      input: A grep command entered by user in command line.
 *
 * Returns:
 *      A string of grep result (contain tailing \n char).
 */
string grep(const string &input);

#endif //GREP_H
