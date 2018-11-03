#!/bin/bash

mkdir dir1
mkdir dir2
mkdir dir3

touch config
touch dir1/f1.bk
touch dir1/f2.sd
touch dir1/f3.txt
touch dir1/fbk

touch dir2/file1
touch dir2/file4

touch dir3/file7

echo "Test" > dir1/f1.bk
echo "$PWD/dir1"> config
echo "$PWD/dir2" >> config
make
./daemon start

sleep 32
echo "dir2 contains only f1.bk"
sleep 5
echo "add file5 to dir 2"

touch dir1/f4.bk
touch dir2/file5

sleep 25
echo "dir2 contains f1.bk f4.bk"

echo "$PWD/dir1"> config
echo "$PWD/dir3" >> config
kill -1 $(pidof ./daemon)

sleep 30
echo "dir3 contains f1.bk f4.bk"

rm dir2/f1.bk

sleep 30
echo "dir2/f1.bk won't be restored"
./daemon stop

rm config
rm -r dir1
rm -r dir2
rm -r dir3