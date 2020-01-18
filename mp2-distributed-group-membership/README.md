# cs425-mp2-distributed-group-membership

# Distributed group membership

This part is the introduction of the distributed group membership.

### Membership List

In this project, each server maintains a membership list, where each entry in the membership list 
is a struct called `Member`, as is shown in the following code block:

```cpp
struct Member {
    char ip_address[14];
    uint64_t time_stamp;
    uint64_t updated_time;
};
```

In a `Member` structure, `ip_address` denotes the ip address of the server in this list, `time_stamp`
 denotes the latest timestamp counter received from the server `ip_address`. The `updated_time` 
 denotes the last refresh time (in milliseconds) for this server to update the `time_stamp` of the 
 server `ip_address`.
 
### Message

There are in total 6 possible messages that will be sent in UDP manner to maintain this membership
list. They're `JOIN`, `LEAVE`, `HEARTBEAT`, `JOIN_SUCCESS`, `FAILURE` and `ANNOUNCE`.


#### Message format

The format of each message is exactly a struct of `Member`, while the `updated_time` entry is 
replaced with value 0-5, respectively denoting the 6 kind of messages:

```cpp
#define JOIN            0
#define LEAVE           1
#define HEARTBEAT       2
#define JOIN_SUCCESS    3
#define FAILURE         4
#define ANNOUNCE        5
```

For each server, it will check whether the `time_stamp` entry for `ip_address` in the received 
message is latest, except for `JOIN`, `ANNOUNCE` and `JOIN_SUCCESS`. If not, it will treat the 
message as out-of-dated and throw it.

#### JOIN

Upon start of each server, user are required to type "join" in order to join the group, including 
indicator. The join message includes the server's own ip address, the local timestamp of the server
machine in milliseconds, and the `updated_time` being 0. After sending `JOIN` message, server waits
for the respond from the indicator.

Only indicator will receive the message with overhead `JOIN`.

#### JOIN_SUCCESS
Upon receive a `JOIN` message, the indicator will add the ip address from the sender of `JOIN` to 
its own membership list, and send back its own entire membership to the sender, with the first 
`Member` element's `updated_time` being `JOIN_SUCCESS` overhead.

Upon receive a `JOIN_SUCCESS` message, the server will use the received membership list to initialize
its own membership list.

Only indicator will send the message with overhead `JOIN_SUCCESS`.


#### HEARTBEAT

Every 0.5 seconds, each server will send a heartbeat message to its successors, and each server will
have at most three successors to send, since at most three machines can fail simultaneously. 

The message to send is a `Member` struct with `time_stamp` being its local time in millisecond and 
`updated_time` being `HEARTBEAT` overhead.

#### LEAVE 

If user types "leave" command in commandline interface, the server will voluntarily leaves the group,
and send a message with `updated_time` being `LEAVE` overhead to its successors. Upon receiving a
non-out-of-dated `LEAVE`, the server will remove the member with `ip_address` in its membership list, 
and continue sending this message to its successors.

#### ANNOUNCE

If indicator receives a `JOIN` message from a new server and let it join the group, it will send a 
message with `ANNOUNCE` overhead and the `ip_address` to be the newly-joined server to its successors, and
let this message to spread in the whole group.

Upon a server receive a `ANNOUNCE` message, it will check whether the `ip_address` is in its membership list. If
not, it will add this message to its membership list, and send this message to its successors.

#### FAILURE

Each server will listen to its three predecessors in the group. If the `time_stamp` of this member is not updated for 3
seconds, meaning that current time minus `updated_time` is greater than 3000ms, it will push this member to a suspected
member list. If a member in the suspected member list is nor heard for more than 4000ms, then the server will denote
it as failure, remove it from its membership list, and send a `FAILURE` message with `ip_address` being the address
of the failed machine to its successors.

Upon receive a not-out-of-dated `FAILURE` message, the server will directly delete the server with `ip_address` from
its membership list and then send this message to its successors.

#### Successors and Predecessors
The Successors and Predecessors of each machine is calculated from the ip address in their membership list
with string order.

