/**
 * client.cpp
 * The client program for entering grep queries.
 */

#include "client_func.h"

using namespace std;

int main(int argc, char const *argv[]) {
    int port_num = 8000;
    auto *my_client = new client(port_num);

    try {
        my_client->run_client();
    } catch (runtime_error &e) {
        cerr << "error: " << e.what() << endl;
        delete my_client;
        return -1;
    }

    delete my_client;
    return 0;
}