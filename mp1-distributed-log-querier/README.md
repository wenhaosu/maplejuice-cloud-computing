# cs425-mp1-distributed-log-querier

# Distributed log querier

This part is the introduction of the distributed log querier.

### Grep

The grep function is implemented in `grep.cpp`, which reads and executes the given command using the system call function in `<stdlib.h>`. If the first word of the command is not 'grep', then a `runtime_error` would be thrown. Otherwise, the result of the grep command is returned.

### Server

The server part of the distributed log querier consists `server.cpp`, `server_func.h` and `server_func.cpp`. `server.cpp` is the main function of grep server, while the detailed functions of server are implemented in `server_func.cpp`.

To run the server in the background, simply type:
```
make server
./server &
```

You can kill the server using:
```
pkill -f server
```

Upon the server is started, it will start listening a specific port for collecting queries (in this project, the port is hardcoded to be 8000). Once the query is received, it will run the grep message and send the result back to the client.
Following are several features of server:

* If any error is detected when running `grep` function (not a grep command, bad grep syntax, no matching results for grep or system fail), the error message will be printed on the server and an empty string will be sent to client as result.

Detailed implementation of server functions can be referred to the comments and docs in `server_func.h` and `server_func.cpp`. 

### Client

The client part of the distributed log querier consists `client.cpp`, `client_func.h` and `client_func.cpp`. `client.cpp` is the main function of grep client, while the detailed functions of server are implemented in `client_func.cpp`.

To run the client, simply type:
```
make client
./client
```

You can kill the server by typing `Ctrl+C`.

Upon the client is started, it will provided a command-line interface for users to input grep queries. 
Once the query is input, client will send the message to all the alive servers and wait for the response (in this project, the IP addresses for servers are hardcoded).
Following are several features of client:

* Users can type `exit` in the client command-line interface to stop the client.
* After the query is sent, client will process the feedback from all servers concurrently.
* Client will create temp files for server feedback and merge all results into a single file called `result.out` at the end. Upon the new query is sent and processed, `result.out` will be refreshed.
* The query result will be stored in `result.out` and the number of lines returned from each server will be printed in the client command-line interface.

Detailed implementation of client functions can be referred to the comments and docs in `client_func.h` and `client_func.cpp`. 

# Unit Test

This part is the introduction of unit test.

### Remember, unit test cannot be used under master branch!

You need to follow the steps below to set up unit test.

## Download unit test branch

To use unit test, please do the following to get the unit test branch

```
git fetch origin unittest
git checkout -b unittest
git pull origin unittest
```

The reason why we make different branches is due to the fact that we need to ajust the client function to enable automatic input and end. However, this will stop the users' input, so we just put it in the unit test branch.

## How to use

There are two main files in the unit test part:

*  `log_file_generator.cpp`
* `test.sh`

### log_file_generator.cpp

It is used to genereate the test logs, corresponding requests and the expected result of the requests.

```
g++ -g -Wall -std=c++11 log_file_generator.cpp -o generator
./generator
```

It will generate in default:

* three test log files: `vm1_test.log`, `vm2_test.log`, `vm3_test.log`
* `test_request`
* `test_answer`

### test.sh

This is a shell script used to automatically run the test.

It will first start the client and input the `test_request`. Then it will compare the output with the `test_answer`.

If the `test_answer` is different from the output, it will output 'wrong!'

If all the test cases are right, it will output 'right!'

### Usage

You need to copy the `vmX_test.log` to the corresponding virtual machine and then rename it use `cp vmX_test.log vm_test.log`

In default, you need to run the server on virtual machines with index 01, 02, 03 and close the others.

Then clone the unit test branch to the virtual machine with client and copy the test_request and test_answer to the client VM.

Then you can run the `test.sh`

If the test_answer is wrong, it will output 'wrong!'

If all the test cases are right, it will output 'right!'