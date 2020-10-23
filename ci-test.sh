#!/bin/bash

./grade.sh
./stop.sh
echo "Test lab2-part3..."

export RPC_COUNT=25

./start.sh && ./test-lab2-part3-a yfs1 yfs2
./stop.sh
./start.sh && ./test-lab2-part3-b yfs1 yfs2
./stop.sh

cat lock_server.log | grep 'RPC STATS' | tail -n1

./start.sh 5 && ./test-lab2-part3-a yfs1 yfs2; ./stop.sh
./start.sh 5 && ./test-lab2-part3-b yfs1 yfs2; ./stop.sh
