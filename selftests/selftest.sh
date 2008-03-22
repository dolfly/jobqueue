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

name="./retval.sh 2"
echo "Running $name test"
echo $name |$com -n2 -r 2>/dev/null
if test "$?" = "0" ; then
    echo "$name test should have failed"
fi

name="./randommigration.py"
echo "Running $name test"
echo $name |$com -n64 -r 2>/dev/null
if test "$?" != "0" ; then
    echo "$name test failed"
fi
