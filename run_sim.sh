#!/bin/bash

set -e
make -j8
trap "echo SIGINT; pkill -P $$" INT

(cd ./samples/cqn/parsim &&
rm -rf comm && mkdir comm comm/read && rm -rf ../../../bin/env/*)

for (( c=0; c<3; c++ ))
do 
(cd ./samples/cqn/parsim &&
../cqn_dbg -n.. -u Cmdenv -c LargeLookahead omnetpp.ini partitioning.ini -p$c --parallel-simulation=true --parsim-communications-class=cNamedPipeCommunications --parsim-synchronization-class=cSimbricksProtocol &)
done

echo Started
sleep 1000