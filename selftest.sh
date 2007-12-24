#!/bin/sh

n=1000
m=10

echo "Running $n jobs in $m processing stations (takes 10 secs or so)"

yes true |head -n $n |./jobqueue -n $m
