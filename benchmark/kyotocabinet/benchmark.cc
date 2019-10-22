#include <kccachedb.h>
#include "cmdcommon.h"
#include "benchmark.h"
#include <getopt.h>
#include <fstream>


// global variables
uint32_t g_randseed;                     // random seed
int64_t g_memusage;                      // memory usage
volatile int stop;
std::vector<std::string> keyArray;
std::vector<std::string> valueArray;
#define TOTAL_KEYS 5000000
#define WRITER_READER_RATIO 5

/////////////////////////////////////////////////////////
// HELPER FUNCTIONS
/////////////////////////////////////////////////////////
static inline int MarsagliaXORV (int x) { 
  if (x == 0) x = 1 ; 
  x ^= x << 6;
  x ^= ((unsigned)x) >> 21;
  x ^= x << 7 ; 
  return x ;        // use either x or x & 0x7FFFFFFF
}

static inline int MarsagliaXOR (int * seed) {
  int x = MarsagliaXORV(*seed);
  *seed = x ; 
  return x & 0x7FFFFFFF;
}

static inline void rand_init(unsigned short *seed)
{
  seed[0] = (unsigned short)rand();
  seed[1] = (unsigned short)rand();
  seed[2] = (unsigned short)rand();
}

static inline int rand_range(int n, unsigned short *seed)
{
  /* Return a random number in range [0;n) */
  
  /*int v = (int)(erand48(seed) * n);
  assert (v >= 0 && v < n);*/
  
  int v = MarsagliaXOR((int *)seed) % n;
  return v;
}

// print members of a database
static void dbmetaprint(kc::BasicDB* db, bool verbose) {
  if (verbose) {
    std::map<std::string, std::string> status;
    status["opaque"] = "";
    status["bnum_used"] = "";
    if (db->status(&status)) {
      uint32_t type = kc::atoi(status["type"].c_str());
      oprintf("type: %s (%s) (type=0x%02X)\n",
              kc::BasicDB::typecname(type), kc::BasicDB::typestring(type), type);
      uint32_t rtype = kc::atoi(status["realtype"].c_str());
      if (rtype > 0 && rtype != type)
        oprintf("real type: %s (%s) (realtype=0x%02X)\n",
                kc::BasicDB::typecname(rtype), kc::BasicDB::typestring(rtype), rtype);
      uint32_t chksum = kc::atoi(status["chksum"].c_str());
      oprintf("format version: %s (libver=%s.%s) (chksum=0x%02X)\n", status["fmtver"].c_str(),
              status["libver"].c_str(), status["librev"].c_str(), chksum);
      oprintf("path: %s\n", status["path"].c_str());
      int32_t flags = kc::atoi(status["flags"].c_str());
      oprintf("status flags:");
      if (flags & kc::CacheDB::FOPEN) oprintf(" open");
      if (flags & kc::CacheDB::FFATAL) oprintf(" fatal");
      oprintf(" (flags=%d)", flags);
      if (kc::atoi(status["recovered"].c_str()) > 0) oprintf(" (recovered)");
      if (kc::atoi(status["reorganized"].c_str()) > 0) oprintf(" (reorganized)");
      oprintf("\n", flags);
      int32_t opts = kc::atoi(status["opts"].c_str());
      oprintf("options:");
      if (opts & kc::CacheDB::TSMALL) oprintf(" small");
      if (opts & kc::CacheDB::TLINEAR) oprintf(" linear");
      if (opts & kc::CacheDB::TCOMPRESS) oprintf(" compress");
      oprintf(" (opts=%d)\n", opts);
      if (status["opaque"].size() >= 16) {
        const char* opaque = status["opaque"].c_str();
        oprintf("opaque:");
        if (std::count(opaque, opaque + 16, 0) != 16) {
          for (int32_t i = 0; i < 16; i++) {
            oprintf(" %02X", ((unsigned char*)opaque)[i]);
          }
        } else {
          oprintf(" 0");
        }
        oprintf("\n");
      }
      int64_t bnum = kc::atoi(status["bnum"].c_str());
      int64_t bnumused = kc::atoi(status["bnum_used"].c_str());
      int64_t count = kc::atoi(status["count"].c_str());
      double load = 0;
      if (count > 0 && bnumused > 0) {
        load = (double)count / bnumused;
        if (!(opts & kc::CacheDB::TLINEAR)) load = std::log(load + 1) / std::log(2.0);
      }
      oprintf("buckets: %lld (used=%lld) (load=%.2f)\n",
              (long long)bnum, (long long)bnumused, load);
      std::string cntstr = unitnumstr(count);
      int64_t capcnt = kc::atoi(status["capcnt"].c_str());
      oprintf("count: %lld (%s) (capcnt=%lld)\n", count, cntstr.c_str(), (long long)capcnt);
      int64_t size = kc::atoi(status["size"].c_str());
      std::string sizestr = unitnumstrbyte(size);
      int64_t capsiz = kc::atoi(status["capsiz"].c_str());
      oprintf("size: %lld (%s) (capsiz=%lld)\n", size, sizestr.c_str(), (long long)capsiz);
    }
  } else {
    oprintf("count: %lld\n", (long long)db->count());
    oprintf("size: %lld\n", (long long)db->size());
  }
  int64_t musage = memusage();
  if (musage > 0) oprintf("memory: %lld\n", (long long)(musage - g_memusage));
}


class ThreadSet : public kc::Thread {
	private:
		int id_;
		kc::BasicDB* db_;
		int rnum_;
		int thnum_;
		unsigned short seed[3];
		bool err_;

	public:
		unsigned long insert;
		unsigned long read;
		void setparam(int id, kc::BasicDB* db, int rnum, int thnum)
		{
			id_ = id;
			db_ = db;
			rnum_ = rnum;
			thnum_ = thnum;
			rand_init(seed);
		}
		void run()
		{
			err_ = false;
			if (thnum_ == 255) {
				for (int i = 0; i < 5000000; i++) {
					char key[16];
					sprintf(key, "%d", i);
					char value[64] = "NULL";
					if (!db_->set(key, 16,
								value, 64)) {
						assert(0 && "Error in setting\n");
					}
				}
				return;
			}


			/*std::ifstream demoFile;
			demoFile.open("demo.txt");
			if (!demoFile.is_open()) {
				printf("Error opening file\n");
				//exit(1);
			}*/
			//std::ofstream demoFileOut;
			//std::string outFileName = "out_" + std::to_string(rnum_) + ".txt";
			//demoFileOut.open(outFileName);
			int count = 0;
			char vbuf[64];
			int range = 20;
			for (int i = rnum_; i < keyArray.size(); i = i + thnum_) {
				std::string kbufString;
				std::string vbufString;
				size_t vsiz;
				if (1) {
					insert++;
					if (!db_->set(keyArray[i].c_str(), keyArray[i].length(),
								valueArray[i].c_str(), valueArray[i].length())) {
						assert(0 && "Error in setting\n");
					}

				}
				count++;
				/*if (count % range != 0) continue;
				int x = 10;
				while(x--) {
					for(int i = 0; i < keysInserted.size(); i++) {
						read++;
						if (!db_->get(keyArray[keysInserted[i]].c_str(), keyArray[keysInserted[i]].length(),
									vbuf, sizeof(vbuf))) {
							assert(0 && "Error in setting\n");
						}
						//demoFileOut << key << " " << vbuf << std::endl;

					}
				}*/
				//range = (rand_range(1000, seed) + 1);
				if (count*thnum_ > TOTAL_KEYS ) break;
			}
		}
};

class ThreadGet : public kc::Thread {
	private:
		int id_;
		kc::BasicDB* db_;
		int rnum_;
		int thnum_;
		unsigned short seed[3];
		bool err_;

	public:
		unsigned long insert;
		unsigned long read;
		void setparam(int id, kc::BasicDB* db, int rnum, int thnum)
		{
			id_ = id;
			db_ = db;
			rnum_ = rnum;
			thnum_ = thnum;
			rand_init(seed);
		}
		void run()
		{
			err_ = false;
			std::ofstream demoFile;
			std::string outFileName = "out_" + std::to_string(rnum_) + ".txt";
			demoFile.open(outFileName);
			int count = 0;
			char vbuf[64];
			std::queue<int> todo;
			int startIndex = rnum_ * 10000;
			int x = rnum_;
			std::string batchString;
			batchString.resize(4000);
			int curLength = 0;
			while (1) {
				for (int i = startIndex; i < (startIndex + 10000) && i < TOTAL_KEYS; i++) {
					char key[16];
					int count = 0;
					sprintf(key, "%d", i);
					size_t keyLength = strlen(key);
try_again:
					memset(vbuf, 0, 64);
					if (!db_->get(key, keyLength,
								vbuf, sizeof(vbuf))) {
						assert(0 && "error in setting\n");
					}
					else read++;
					if(strncmp(vbuf, "NULL", 4) == 0) {
						count++;
						if (count < 100)
							goto try_again;
						else continue;
					}else {
						//demoFile << key << " " << vbuf << std::endl;
						std::string appendString = std::string(key) + " " + std::string(vbuf) + "\n";
						if (curLength + appendString.length() < 4000) {
							batchString.append(std::string(appendString));
							curLength += appendString.length();
						}else {
							demoFile << batchString;
							batchString.clear();
							batchString.append(appendString);
							curLength = 0;
						}
					}
				}
				x += thnum_;
				startIndex = (x) * 10000;
				if (startIndex > TOTAL_KEYS) break;
			}
			/*
			while (1) {
				int key = count*thnum_ + rnum_ + 1;
				std::string kbufString = std::to_string(key);
				if (!db_->get(kbufstring.c_str(), kbufstring.length(),
							vbuf, sizeof(vbuf))) {
					assert(0 && "error in setting\n");
				}

				count++;
				if (count > 100000) break;
			}*/
		}
};

class ThreadBench : public kc::Thread {
	private:
		int id_;
		kc::BasicDB* db_;
		int rnum_;
		int thnum_;
		int update_;
		bool err_;
		unsigned short seed[3];

	public:
		long add;
		long remove;
		long get;

		void setparam(int id, kc::BasicDB* db, int rnum, int update, int thnum)
		{
			id_ = id;
			db_ = db;
			rnum_ = rnum;
			thnum_ = thnum;
			update_ = update;
			add = 0;
			remove = 0;
			get = 0;
			rand_init(seed);
		}
		void run()
		{
			err_ = false;
			int range = rnum_ * 2;
			while (!stop) {
				int op = rand_range(1000, seed);
				char kbuf[64];
				size_t ksiz = sprintf(kbuf, "%08lld", 
						(long long)rand_range(range+1, seed));
				if (op < update_) {
					if (1 || (op & 0x01) == 0) {
						db_->set(kbuf, ksiz, kbuf, ksiz);
						add++;
					} else {
						db_->remove(kbuf, ksiz);
						remove++;
					}
				} else {
					char vbuf[64];
					int vsiz = db_->get(kbuf, ksiz, vbuf, sizeof(vbuf));
					get++;
				}
			}
			printf("%ld %ld %ld\n",add, remove, get);
		}
};

void print_usage() {
	printf("Usage: ./benchmark -t num\n");
}

inline double get_now() {
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec + tv.tv_usec / 1000000.0;
}

int main(int argc, char *argv[])
{
	int thnum = -1;
	int option = 0;
	while ((option = getopt(argc, argv, "t:")) != -1) {
		switch(option) {
			case 't': thnum = atoi(optarg);
				  break;
			default: print_usage();
				 exit(EXIT_FAILURE);
		}
	}
	if (thnum < 1) {
		print_usage();
		exit(EXIT_FAILURE);
	}
	g_memusage = memusage();
	kc::CacheDB db;
	printf("opening database:\n");
	//db.tune_logger(stdlogger(g_progname, &std::cout),
	//		lv ? kc::UINT32MAX : kc::BasicDB::Logger::WARN | kc::BasicDB::Logger::ERROR);

	if (!db.open("*", kc::CacheDB::OWRITER | kc::CacheDB::OCREATE | kc::CacheDB::OTRUNCATE)) {
		printf("Error Opening DB\n");
		assert(0);
	}
	std::ifstream demoFile;
	demoFile.open("demo.txt");
	if (!demoFile.is_open()) {
		printf("Error opening file\n");
		//exit(1);
	}
	while (demoFile.good()) {
		std::string kbufString;
		std::string vbufString;
		demoFile >> kbufString >> vbufString;
		if (kbufString.length() == 0 || vbufString.length() == 0) {
			continue;
		}
		keyArray.push_back(kbufString);
		valueArray.push_back(vbufString);
	}

	int rnum = RECORD_NUM;
	int update = UPDATE_RATIO;
	int duration = DURATION;

	struct timespec timeout;
	timeout.tv_sec = duration / 1000;
	timeout.tv_nsec = (duration % 1000) * 1000000;
	
	printf("Setting Records\n");
	

	ThreadSet threadsets[THREADMAX];
	ThreadGet threadgets[THREADMAX];
	threadsets[0].setparam(255, &db, 255, 1);
	threadsets[0].start();
	threadsets[0].join();
	double start_time = get_now();
	for (int i = 0; i < WRITER_READER_RATIO*thnum; i++) {
		threadsets[i].setparam(i, &db, i, WRITER_READER_RATIO*thnum);
		threadsets[i].start();
	}
	for (int i = 0; i < thnum; i++) {
		threadgets[i].setparam(i, &db, i, thnum);
		threadgets[i].start();
	}
	for(int i = 0; i < WRITER_READER_RATIO * thnum; i++)
		threadsets[i].join();
	for(int i = 0; i < thnum; i++)
		threadgets[i].join();
	double end_time = get_now();
	printf("duration = %f\n", end_time - start_time);
	unsigned long total_op = 0;
	for (int i = 0; i < WRITER_READER_RATIO *thnum; i++) 
		total_op += threadsets[i].insert + threadsets[i].read;
	for (int i = 0; i < thnum; i++) 
		total_op += threadgets[i].insert + threadgets[i].read;

	printf("PASS\nSummary: ");


	printf("total_ops=%f\n", total_op * 1000 /(end_time - start_time) / 1000000.0);
	/*for (int i = 0; i < thnum; i++) {
		threadgets[i].setparam(i, &db, i, thnum);
		threadgets[i].start();
	}*/
	/*for(int i = 0; i < thnum; i++)
		threadgets[i].join();*/

	
	printf("Done setting records\n");

	stop = 0;

	dbmetaprint(&db, true);
	printf("Closing the database\n");
	if (!db.close())
		printf("Error in closing\n"); 
		

}
