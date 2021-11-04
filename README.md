MV-RLU: Scaling Read-Log-Update with Multi-Versioning
====================================================

This repository contains the code for MV-RLU, a scalable
synchronization mechanism. MV-RLU has been test in Linux with Intel
Xeon Processor.

## Directory structure
```{.sh}
mv-rlu
├── include             # public headers of mvrlu
├── lib                 # mvrlu library
├── benchmark           # benchmark
│   ├── DBx1000         #  - DBx1000
│   ├── rlu             #  - rlu implementation and benchmark
│   ├── kyotocabinet    #  - kyotocabinet benchmark
│   └── versioning      #  - versioned programming and benchmark
├── bin                 # all binary files and scripts
└── tools               # misc build tools
```

## Prerequisites
- Recent gcc (8.0+)
- gnu-make (4.0+)

## Setting Ordo value
Before building, you need the ordo value for your machine
```{.sh}
$> make ordo
```
Copy the ordo value to `include/ordo_clock.h`
``` c
#define __ORDO_BOUNDARY (<ORDO_VALUE_HERE>)
```

## How to configure, build, test, and clean
```{.sh}
$> make
```

## Running benchmarks
All the binary files are store in the `./bin` folder. To use the
automated python script follow the README in the `bin` folder

## Other resources
- [ASPLOS 2019 paper](https://dl.acm.org/citation.cfm?id=3304040)

## Authors
- Jaeho Kim [jaeho@vt.edu](mailto:jaeho@vt.edu)
- Ajit Mathew [ajitm@vt.edu](mailto:ajitm@vt.edu)
- Sanidhya Kashyap [sanidhya@gatech.edu](mailto:sanidhya@gatech.edu)
- Madhava Krishnan Ramanathan [madhavakrishnan@vt.edu](mailto:madhavakrishnan@vt.edu)
- Changwoo Min [changwoo@vt.edu](mailto:changwoo@vt.edu)
