#!/bin/bash
export LD_LIBRARY_PATH="/homes/praneethbala/omnetpp-6.0.1/lib:/homes/praneethbala/simbricks/lib:$LD_LIBRARY_PATH"
export PATH="/homes/praneethbala/omnetpp-6.0.1/bin:$PATH"
set -e
make -j16
trap "echo SIGINT; pkill -f fat_tree_simple" INT

(cd ./samples/fat_tree_simple-$1 &&
cp $2_processes.txt omnetpp.ini && rm -rf /dev/shm/env/*)

(cd ./samples/fat_tree_simple-$1 &&
mpirun -n $2 --oversubscribe ./fat_tree_simple -n. -u Cmdenv --parallel-simulation=true --parsim-communications-class=cMPICommunications --parsim-synchronization-class=cSimbricksProtocol &)

echo Started
sleep 10000000
