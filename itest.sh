# ./stop.sh
# make && ./start.sh && \
# # ./test-lab1-part2-e.sh ./yfs1
# dd if=tmprand of=yfs1/fooi.txt bs=1K seek=3 count=30 >/dev/null 2>&1
# echo "dd returned: $?"
# ./stop.sh

./stop.sh
./start.sh
ln --verbose -s /etc/hosts ./yfs1/test 
./stop.sh
