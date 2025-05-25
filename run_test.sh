#!/usr/bin/env bash

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
# cmake -B build-pgo-gen -DCMAKE_BUILD_TYPE=Release ..
# cmake --build build-pgo-gen -j
# export GCOV_PREFIX=$(pwd)/pgo_data
# export GCOV_PREFIX_STRIP=0
# mkdir -p "$GCOV_PREFIX"
cd ..
# ./runner  -disable-hugepages -b rest_buffer -sol ./build/build-pgo-gen/solution -o out.log -meta lat-spring-data/public1.meta lat-spring-data/public1.pcapng
./runner  -disable-hugepages -b rest_buffer -sol ./build/solution -o out.log -meta lat-spring-data/public1.meta lat-spring-data/public1.pcapng

# for i in {1..100}; do
#     echo "Запуск №$i"
#     ./runner  -disable-hugepages -b rest_buffer -sol ./build/solution -o out.log -meta lat-spring-data/public1.meta lat-spring-data/public1.pcapng
#     echo "Завершено №$i"
#     echo "-----------------------------"
# done

# sudo perf script --comm=solution > out.perf
# sudo perf record -F 20000 -a -g -e cycles:u --call-graph dwarf --inherit   -- ./runner  -disable-hugepages -b rest_buffer -sol ./build/solution -o out.log -meta lat-spring-data/public1.meta lat-spring-data/public1.pcapng
# sudo perf script --comm=solution > out.perf
# ./../../Desktop/FlameGraph/stackcollapse-perf.pl out.perf | ./../../Desktop/FlameGraph/flamegraph.pl > flamegraph.svg
# google-chrome flamegraph.svg
python3 py.py
diff out_trimed.log lat-spring-data/public1_trimed.res > diff.log
# rm -rf build
# rm -rf out.log
rm -rf out_trimed.log


