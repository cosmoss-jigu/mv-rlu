# Benchmark
Use this folder to run linked list and hash-list benchmark.

## Script
`run_bench.py` can be used to automate benchmarking.
Use `python run_bench.py -h` for help
Matplotlib is required.

## Configuration
`run_bench.py` reads the benchmark configuration from a json file
which can be specified using -c option (by default it will read config.json).

The fields of the json file are as follows:
- `data_structure`: llist or hlist
- `run_per_test`: Number of runs per test
- `rlu_max_ws`: Set to 1
- `buckets`: Number of buckets in hash table. If benchmarking
linked list set it to 1
- `alg_type`: Possible values:
        - mvrlu_ordo : mvrlu with physical timestamping
        - rcu : read copy update
        - rlu : rlu with logical timestamping
        - harris: Harris with no garbage collection
        - hp_harris: Harris with hazard pointer
- `update_rate`: 2% update = 20, 20% update = 200, 80% update = 800
- `initial_size`: Number of initial nodes in data structure
- `range_size`: Key range. Should be atleast 2x the initial size.
- `zipf_dist_val`: value of theta
- `threads`: List of threads

### Sample Config File

```json

{
	"hlist1k": [
                {
                        "data_structure": "hlist",
                        "runs_per_test": 1,
                        "rlu_max_ws" : 1,
                        "buckets" : 1000,
                        "duration" : 20000,
                        "alg_type" : ["mvrlu_ordo", "rcu", "rlu", "harris", "hp_harris"],
                        "update_rate" : [0, 20, 200, 800],
                        "initial_size" : 1000,
                        "range_size" : 2000,
                        "zipf_dist_val" : 0,
                        "threads" : [448, 392, 336, 280, 224, 196, 168, 140, 112, 84, 56, 28, 14, 8, 4, 1]
                }
        ]
}
```
