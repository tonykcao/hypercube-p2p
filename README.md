
# Introduction

This project is a proof of concept Peer-to-peer chat client using hypercubic
topology. It is meant to examine logical hop count behavior, topology maintenance,
and fault tolerance of such a network under an idealized environment (equal latency between nodes).


## Source code
This software was developed for CS87 at Swarthmore College under the guidance of professor Tia Newhall. Permission to publish this material has been explicitly granted by the instructor.
The source code has been redacted to remove material not developed by the authors.


## How To Use
To clone and run this application, use the provided Makefiles in the subdirectories.
The subdirectories includes copies of clients using client-server model, full mesh topology,
and hypercubic topology with "supernodes" assisting topology maintenance.

## Testing
Use the included Makefile to compile the include files, modify metadata sending for messages as needed. Some python scripts we used are also included here.
A histogram of logical hop counts we produced is included here. It was tested on a 256 clients network, with similar latency
between every nodes. Here, the network is fully formed then each node send 1 message per second, then after 100 seconds nodes begin to randomly drop out and topology is maintained by supernodes.

## Credits

This program was co-authored with Xinyu Dong '24 and Derrick Zhen '24.

## License

See included license file.

---

> GitHub [@tonykcao](https://github.com/tonykcao) &nbsp;&middot;&nbsp;

