#!/bin/sh

com=../jobqueue

n=1000
m=10

echo "Running $n jobs in $m processing stations (takes 10 secs or so)"

yes true |head -n $n |$com -n $m 2>/dev/null

echo "Running deadlock test"
for i in $(seq 25) ; do
    yes ./randomfailure.py |head -n1 |$com -n2 2>/dev/null
done

echo "Running deadlock test with -r"
for i in $(seq 25) ; do
    yes ./randomfailure.py |head -n1 |$com -n2 -r 2>/dev/null
done
