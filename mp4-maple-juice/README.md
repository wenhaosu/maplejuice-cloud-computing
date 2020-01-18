# cs425-mp4-maple-juice

# MapleJuice

This part is the introduction of the MapleJuice framework.

### Framework Overview

The purpose of this project is to build a new parallel cloud computing framework called MapleJuice, 
which bears similarities to MapReduce. The client can now submit maple (map) and juice (reduce) jobs to the server, 
and let server run the maple and juice jobs distributively on other servers.

Additionally, client can submit several maple and juice tasks simultaneously, but all those jobs are down sequentially
in this distributed system.

### Maple Phase

The first phase, Maple, is invokable from the command line as:
```bash
maple <maple_exe> <num_maples> <sdfs_intermediate_filename_prefix> <sdfs_src_directory>
```

The first parameter `maple_exe` is a user-specified executable that takes as input one
file and outputs a series of (key, value) pairs. `maple_exe` is the file name on the sdfs. The last series
of parameters (`sdfs_src_directory`) specifies the location of the input files. `sdfs_intermediate_filename_prefix` is
the prefix names of the intermediate output files of maple phase. `num_maples` denote the number of workers that will
do the maple job.


Upon receive the maple command from client, the master node will first select the worker nodes to assign the maple 
query. (If the number of nodes is less then `num_maples`, use all nodes to run the job). For each node, we assign a 
specific part of input files to it based on hash-based partition or range-based partition. We also assign a mission number
to it. We then maintain a struct for each divided maple mission:
```cpp
class MapleMission {
public:
    int mission_id;
    Stage phase_id;
    vector<string> files;
};
```
Here `Stage` is an enumerator:
```cpp
enum Stage {
    PHASE_I, PHASE_II, PHASE_III, PHASE_IV
};
```

The master node will then encode the MapleMission class to a string and send to the target worker.

Upon the worker receives the maple query from master, it will decode the string to find the original MapleMission class.
Then it will start executing the maple job.

#### Maple Phase_I to PHASE_II
After receiving the maple query, the worker node will decode the query, and obtain the required files, namely the `maple_exe`
file and all input source files from sdfs. After that, it will send back an ack to master, saying `maple_mission_receive`.
 After receiving this ack, master node will update the `Stage` of this mission to `PHASE_II`.
 
#### Maple Phase_II to PHASE_III
The worker node will then begin to execute the maple job using system command. Here we process 10 lines from the source
file at a time: we read the source file, extract 10 lines and pass it to the `maple_exe`. We then dump the output of 
`maple_exe` to a local file called `result`. After all input source files are processed, the worker node will read the 
`result` file line by line and split it to at most 10 files, with the name of each file to be `sdfs_intermediate_filename_prefix` 
followed by the hashed value of the key. When the split is done, it will send back an ack to master, saying `maple_mission_finished`.
After receiving this ack, master node will update the `Stage` of this mission to `PHASE_III`.

#### Maple Phase_III to PHASE_IV
Then the worker node will begin to upload all the intermediate files to sdfs. The name of the intermediate files are as following:
If the prefix is `wcout` and the mission number is `1`, then the intermediate files will be `wcout_key_1` while the `key` 
ranges from 0 to 9. This can also be thought as a pre hash partition for the reduce stage. When all files are uploaded to 
the sdfs, it will send back an ack to master, saying `maple_result_uploaded`. After receiving this ack, master node will 
update the `Stage` of this mission to `PHASE_IV`.

When all workers finish their missions, the master node will mark this maple job as finished and send back to the client 
that this maple job is done.


### Juice Phase

The second phase, Juice, is invokable from the command line as:
```bash
juice <juice_exe> <num_juices> <sdfs_intermediate_filename_prefix> <sdfs_dest_filename> delete_input={0,1}
```

The first parameter `juice_exe` is a user-specified executable that takes as input multiple
(key, value) input lines, processes groups of (key, any_values) input lines together
(sharing the same key, just like Reduce), and outputs (key, value) pairs. `juice_exe` is
the file on the sdfs. The second parameter `num_juices` specifies the number of Juice tasks. `sdfs_intermediate_filename_prefix` is
the prefix names of the intermediate output files of the previous maple phase.

Upon receive the juice command from client, the master node will first select the worker nodes to assign the juice 
query. (If the number of nodes is less then `num_juices`, use all nodes to run the job). For each node, we assign a 
specific part of input files to it based on mission number of the previous maple output files. We also assign a 
mission number to it. We then maintain a struct for each divided juice mission:
```cpp
class JuiceMission {
public:
    int mission_id;
    Stage phase_id;
    vector<string> prefixes;
};
```

The master node will then encode the JuiceMission class to a string and send to the target worker.

Upon the worker receives the juice query from master, it will decode the string to find the original JuiceMission class.
Then it will start executing the juice job.

#### Juice Phase_I to PHASE_II
After receiving the juice query, the worker node will decode the query, and obtain the required files, namely the `juice_exe`
file and all input source files from sdfs. After that, it will send back an ack to master, saying `juice_mission_receive`.
 After receiving this ack, master node will update the `Stage` of this mission to `PHASE_II`.

#### Juice Phase_II to PHASE_III
The worker node will then begin to execute the jucie job using system command. The whole process is similar to maple,
while the output is a whole file that does not need to split. When the job is done, it will send back an ack to master, 
saying `jucie_mission_finished`. After receiving this ack, master node will update the `Stage` of this mission to `PHASE_III`.

#### Juice Phase_III to PHASE_IV
Then the worker node will begin to upload its intermediate file to sdfs. The name of the intermediate file are as following:
If the prefix is `wcout` and the mission number is `1`, then the intermediate files will be `wcout_1`. When all files are 
uploaded to the sdfs, it will send back an ack to master, saying `juice_result_uploaded`. After receiving this ack, master node will 
update the `Stage` of this mission to `PHASE_IV`.

When all workers finish their missions, the master node will mark this juice job as finished and send back to the client 
that this juice job is done.

### Mission Redistribution

We also does error handling to MapleJuice. For each maple and juice mission, we maintain a query of free workers. When
a worker is crashed, the socket from this worker to the master will be disconnected, meaning that the return value of
`recv` will be 0. When the master node monitored a 0 in `recv`, it will throw an error. In the catch stage, the master node
will keep checking whether the free worker queue is not empty. If there is a free worker, then the master will send the 
mission structure to that worker. Unless all the workers are died, there will always be a worker doing the redistributed
mission, and the task will finally be done.
