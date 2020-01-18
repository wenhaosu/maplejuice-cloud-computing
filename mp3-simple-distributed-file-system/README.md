# cs425-mp3-simple-distributed-file-system

# Distributed file system

This part is the introduction of the distributed file system.

### Masters and Slaves

In this machine, for each sdfs cloud file, a machine is either a master of this file or a slave of this file.
We determine the master server of a file according the hash value of the file name, and let the slave servers
of this file be the successors of the master server on the virtual ring (namely, the heartbeat sending targets).

For each sdfs file, we store 4 replicas of it, with one on the master server and the rest on each slave server.
 Master servers are responsible for operations such as rearranging the files when certain nodes join or leaves.

### SDFS File List

In this project, each machine has a directory called `files` in the program executing path, where there're
three folders inside it: `local`, `fetched` and `sdfs`, which means the local files, fetched files from sdfs cloud,
and the sdfs cloud files that stored on this machine.

Each server maintains a sdfs file list `stored_sdfs_files`, which is a has table storing `FileInfo` structs 
holding information for all sdfs files currently storing on this machine.

```cpp
struct FileInfo {
    bool is_master;
    uint64_t time_stamp;
    uint64_t file_size;
};
```

In a `FileInfo` structure, `is_master` denotes whether this server is the master server of this sdfs file
, `time_stamp` denotes the latest timestamp counter received the file, `file_size` 
 denotes the size of this sdfs file.
 
### Queries
 
There are five kind of queries that client can send to server to interact with the sdfs file system.
 
#### put 
Syntax: `put localfilename sdfsfilename`
 
Load a file with `localfilename` from `files/local` and stores it to the sdfs file system. The server that receives
 this query will first check the ip addresses of master and slaves for this file according to hash value and
 virtual ring, then use TCP connection to send the file.
 
We use quorum with W = 4 to handle `put`. A `put` is considered success when all servers storing this file
 returns put success.

If two updates (puts) to one file are initiated within 1 minute of each other, the user doing the second 
file update should be prompted to confirm whether she really wants to push that file update.
 
#### get 
Syntax: `get sdfsfilename localfilename`

Download a file with `sdfsfilename` in sdfs file system and stores it to `files/fetched` with the name `localfilename`. 
The server that receives this query will first check whether this file exists, then use TCP connection to get the file.

We use quorum with R = 1 to handle `get`. A `get` is considered success when a server storing this file
 returns get success.
 
#### delete 
Syntax: `delete sdfsfilename`

Delete a file with `sdfsfilename` in sdfs file system. The server that receives this query will first check whether this 
file exists, then use TCP connection to let all servers that have this file to remove it.

We use quorum with W = 4 to handle `delete`. A `delete` is considered success when all servers storing 
this file returns delete success.

#### ls
Syntax: `ls sdfsfilename`

List all machine ip addresses where the `sdfsfilename` is currently being stored. The server that receives this query will first check whether this 
file exists, then use TCP connection to let all servers that have this file to return a message.

#### store
Syntax: `store`

List all sdfs files that are currently stored on this server.

### Rearrange

One challenge of implementing sdfs is to rearrange sdfs files when new machines join or leave. Here we consider 
two situations. After each rearrange operation, we make sure that all files are stored in their hashed
 masters and slaves.

#### Leave
This is related to the `LEAVE` and `FAILURE` cases in `distributed-group-membership` membership list. When 
a leave occurs, each machine first all the files on its `stored_sdfs_files`.

* If this machine is a file master and the leave action refreshed its slaves list, then it send a put to its new slave.
* If this machine should become a new file master, then it send a put to its slaves that do not have the file.

#### Join
This is related to the `JOIN` and `ANNOUNCE` cases in `distributed-group-membership` membership list. When 
a leave occurs, each machine first all the files on its `stored_sdfs_files`.

* If this machine is a file master and the join action refreshed its slaves list, then it send a put to its new slave 
and send a delete to its old slave.
* If this machine is no longer a file master, then it send a put to the new file master and send a delete to the
 slave of it that should no longer hold the file.
