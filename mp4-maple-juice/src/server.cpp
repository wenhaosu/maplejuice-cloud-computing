/**
 * server.cpp
 * The server program for processing grep queries.
 */

#include "server_func.h"

using namespace std;

int main(int argc, char const *argv[]) {
    ios_base::sync_with_stdio(false);
    auto *my_server = new server();

    try {
        my_server->run_server();
    } catch (runtime_error &e) {
        cerr << "error: " << e.what() << endl;
        delete my_server;
        return -1;
    }

    delete my_server;
    return 0;
}
