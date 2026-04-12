#!/bin/sh

#
# basic map-reduce test for C++ version
#

# run the test in a fresh sub-directory.
rm -rf mr-tmp
mkdir mr-tmp || exit 1
cd mr-tmp || exit 1
rm -f mr-*

# compile
(cd /6.824/Lab1_MapReduce/build && cmake .. && make) || exit 1


failed_any=0

# ----------------------------------------------- 1. Word Count Test ----------------------------------------------------

# generate the correct output
../mrsequential ../wc.so ../pg*txt || exit 1
sort mr-out-0 > mr-correct-wc.txt
rm -f mr-out*

echo '***' Starting wc test.

../../build/master 1 ../pg*.txt &

# give the master time to create the sockets.
sleep 1

# start multiple workers.
../../build/worker ../wc.so &
../../build/worker ../wc.so &
../../build/worker ../wc.so &

wait

sort mr-out* | grep . > mr-wc-all

if cmp mr-wc-all mr-correct-wc.txt
then
  echo '---' wc test: PASS
else
  echo '---' wc output is not the same as mr-correct-wc.txt
  echo '---' wc test: FAIL
  failed_any=1
fi


# ----------------------------------------------- 2. Indexer Test ----------------------------------------------------
# wait for remaining workers and master to exit.
wait ; wait ; wait

# now indexer
rm -f mr-*

# generate the correct output
../mrsequential ../indexer.so ../pg*txt || exit 1
sort mr-out-0 > mr-correct-indexer.txt
rm -f mr-out*

echo '***' Starting indexer test.

../../build/master 8 ../pg*.txt &
sleep 1

# start multiple workers
../../build/worker ../indexer.so &
../../build/worker ../indexer.so &

wait

sort mr-out* | grep . > mr-indexer-all
if cmp mr-indexer-all mr-correct-indexer.txt
then
  echo '---' indexer test: PASS
else
  echo '---' indexer output is not the same as mr-correct-indexer.txt
  echo '---' indexer test: FAIL
  failed_any=1
fi