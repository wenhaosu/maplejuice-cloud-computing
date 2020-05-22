# MapleJuice Cloud Computing
MapleJuice is a course project from UIUC CS425: Distributed Systems. It is divided into 4 stages:

1. Setting up a distributed log querier where we can grep information from log files stored on multiple machines. (Detail explained in [MP1 Report](https://github.com/WenhaoSu/maplejuice-cloud-computing/blob/master/reports/MP1%20report%20Wenhao%20Su(wenhaos3)%20Yichen%20Yang(yy18).pdf))
2. Utilizing heart-beating failure detection to maintain a dynamic p2p membership list with high-reliability fault tolerance. (Detail explained in [MP2 Report](https://github.com/WenhaoSu/maplejuice-cloud-computing/blob/master/reports/MP2%20report%20Wenhao%20Su(wenhaos3)%20Yichen%20Yang(yy18).pdf))
3. Building a scalable distributed file system (DFS) which supports get, put, delete operations with replica control. (Detail explained in [MP3 Report](https://github.com/WenhaoSu/maplejuice-cloud-computing/blob/master/reports/MP3%20report%20Wenhao%20Su(wenhaos3)%20Yichen%20Yang(yy18).pdf))
4. Developing a parallel cloud computing framework similar to MapReduce, with which master node assigns data processing jobs to worker nodes and handles job reassignment when there are node failures. This framework guarantees correctness, and is generally faster then Hadoop when the data size is relatively small. (Detail explained in [MP4 Report](https://github.com/WenhaoSu/maplejuice-cloud-computing/blob/master/reports/MP4%20report%20Wenhao%20Su(wenhaos3)%20Yichen%20Yang(yy18).pdf))

## Usage

The full project of MapleJuice Cloud Computing framework is in `mp4-maple-juice`. The detailed usages for each stage are explained in the `README.md` in their corresponding folders.
