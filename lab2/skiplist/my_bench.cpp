#include <iostream>
#include <chrono>
#include "memdb.h"

using namespace std;
using namespace std::chrono;

void run_test(size_t mem_size) {
    MemDBOptions opt;
    opt.max_memtable_bytes = mem_size;

    InMemoryDB db(opt);

    int N = 100000;

    auto start = high_resolution_clock::now();

    for (int i = 0; i < N; i++) {
        db.Put(i, "value");
    }

    auto end = high_resolution_clock::now();

    cout << "MemTable: " << mem_size / 1024 / 1024 << "MB → ";
    cout << duration_cast<milliseconds>(end - start).count() << " ms\n";
}

int main() {
    run_test(1 * 1024 * 1024);
    run_test(4 * 1024 * 1024);
    run_test(16 * 1024 * 1024);
}