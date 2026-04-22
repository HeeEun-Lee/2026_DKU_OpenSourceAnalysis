#include <iostream>
#include <chrono>
#include "memdb.h"

using namespace std;
using namespace std::chrono;

void run_test(float p) {
    MemDBOptions opt;
    opt.max_memtable_bytes = 4 * 1024 * 1024; // 고정 (4MB)
    opt.skiplist_p = p;

    InMemoryDB db(opt);

    int N = 100000;

    auto start = high_resolution_clock::now();

    for (int i = 0; i < N; i++) {
        db.Put(i, "value");
    }

    auto end = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>(end - start);

    cout << "p = " << p << " -> " 
         << duration.count() << " ms" << endl;
}

int main() {
    run_test(0.25f);
    run_test(0.5f);
    run_test(0.75f);

    return 0;
}