#!/bin/sh

com=../jobqueue

n=500
m=10

echo "Running $n jobs in $m processing stations"

yes true |head -n $n |$com -n $m

function deadlocktest() {
    echo "Running deadlock test with" "$@"
    for i in $(seq 25) ; do
	yes ./randomfailure.py |head -n1 |$com -n2 "$@"
    done
}

deadlocktest
deadlocktest -r
deadlocktest --max-restart=1
