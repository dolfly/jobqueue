#!/bin/sh

n=10000
m=10

echo "Running $n jobs in $m processing stations (takes a minute or so)"

yes true |head -n $n |./jobqueue -n $m
