#include <benchmark/benchmark.h>

#include <cstdlib>
#include <list>
#include <sstream>
#include <iostream>
using namespace std;
typedef unsigned long long inum;

std::string filename(inum inum) {
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

typedef struct _dirent {
    std::string name;
    unsigned long long inum;
} dirent;

static void BM_StringCreation(benchmark::State& state) {
    std::string fname = "filename";
    inum ino = 1;
    for (auto _ : state) {
        std::string package(fname);
        package.push_back('/');
        package.append(filename(ino) + "/");
    }
}
// Register the function as a benchmark
// BENCHMARK(BM_StringCreation);

static void BM_DIR(benchmark::State& state) {
    std::string fname = "filename";
    inum ino = 1;
    for (auto _ : state) {
        std::string package(fname);
        char buf[10];
        sprintf(buf, "/%llu/", ino);
        package.append(buf);
    }
}
// Register the function as a benchmark
// BENCHMARK(BM_DIR);

static void BM_DIR_m(benchmark::State& state) {
    std::string fname = "filename";
    inum ino = 1;
    for (auto _ : state) {
        std::string package(fname);
        // package.push_back('/');
        char buf[1000];
        sprintf(buf, "/%llu/", ino);
        package.append(buf);
    }
}
// Register the function as a benchmark
// BENCHMARK(BM_DIR_m);

static void BM_DIR_L(benchmark::State& state) {
    std::string fname = "filename";
    inum ino = 1;
    for (auto _ : state) {
        // std::string package(fname);
        // package.push_back('/');
        char buf[100];
        sprintf(buf, "%s/%llu/", fname.c_str(), ino);
        // package.append(buf);
        std::string package(buf);
    }
}
// Register the function as a benchmark
// BENCHMARK(BM_DIR_L);

// Define another benchmark
static void BM_StringCopy(benchmark::State& state) {
    std::string fname = "filename";
    inum ino = 1;
    for (auto _ : state) {
        std::string package(fname);
        package.push_back('/');
        char buf[10];
        sprintf(buf, "%llu", ino);
        package.append(buf);
        package.push_back('/');
    }
}
// BENCHMARK(BM_StringCopy);

static void BM_ULL2STR(benchmark::State& state) {
    inum inum = 2333;
    for (auto _ : state) {
        std::ostringstream ost;
        ost << inum;
        ost.str();
    }
}

static void BM_ULL2STR2(benchmark::State& state) {
    inum ino = 2333;
    for (auto _ : state) {
        std::ostringstream ost;
        ost << ino;
        ost.str();
    }
}

// BENCHMARK(BM_ULL2STR);

static void parse(benchmark::State& state) {
    std::string buf = "n_inode_alloc-12/123/";
    std::list<dirent> list;
    for (auto _ : state) {
        while (!buf.empty()) {
            size_t pos = buf.find('/');
            std::string fname = buf.substr(0, pos);
            buf.erase(0, pos + 1);
            pos = buf.find('/');
            std::string ino = (buf.substr(0, pos));
            buf.erase(0, pos + 1);
            dirent e;
            e.name = fname;
            e.inum = atoi(ino.c_str());
            list.push_back(e);
        }
    }
}

static void parse2(benchmark::State& state) {
    std::string buf = "n_inode_alloc-12/123/b/2/";
    std::list<dirent> list;
    for (auto _ : state) {
        std::vector<std::string> results;
        std::string::const_iterator start = buf.begin();
        std::string::const_iterator end = buf.end();
        std::string::const_iterator next;
        while (start != end) {
            // cout << "start: " << *start << endl;
            dirent e;
            next = std::find(start, end, '/');
            e.name = std::string(start, next);
            // cout << e.name << endl;
            std::string::const_iterator l = next + 1;
            next = std::find(l, end, '/');
            e.inum = atoi(std::string(l, next).c_str());
            // cout << e.inum << endl;
            list.push_back(e);
            start = next + 1;
            // cout << *start << " next: " << *next << (next == end) << (start == end) << endl;
        }
    }
}

BENCHMARK(parse);
BENCHMARK(parse2);

BENCHMARK_MAIN();
