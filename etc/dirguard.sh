#!/bin/bash
# This script keeps a lock on the current directory until the parent process
# exits. If this script is run, and find an existing lock file, then waits for
# that lock file to disappear before proceeding. After the lock file is created
# successfully, the script returns.

echo Looking for lock file in `pwd` >&2

function waitthread() {
    WATCHPID=$1
    MYLFILE=dirlock.$BASHPID    
    while true ; do
        echo "LWATCHPID=$WATCHPID" > $MYLFILE
        echo "LWATCHERPID=$BASHPID" >> $MYLFILE
        while [ -f $MYLFILE ] ; do
            while source dirlock 2>/dev/null ; do
                if ps $LWATCHERPID > /dev/null ; then
                    sleep 0.1
                else
                    echo DELETING LOST LOCK in `pwd` for pid $LWATCHERPID >&2
                    rm dirlock
                fi
            done
        # mv is an atomic operation on the filesystem. if it fails, the outer
        # while loop will still find our own lock file and go another trip of
        # waiting for the (now new) master lock.
            mv -n $MYLFILE dirlock
        done
        sleep 0.1
        source dirlock 2>/dev/null
        if [ "$LWATCHERPID" == "$BASHPID" ] ; then
            break ;
        else
            echo FALSE LOCK ACQUIRED for `pwd` me $BASHPID lock $LWATCHERPID >&2
        fi
    done
    # Now: we have the lock.
    echo acquired
    # wait for the parent PID to disappear
    while ps $WATCHPID > /dev/null ; do
        sleep 0.1
    done
    # give back the lock
    rm dirlock 2>/dev/null || echo ERROR REMOVING LOCK FOR pid $BASHPID >&2
}


coproc waitthread $PPID

NUM=${COPROC[0]}
read dd <&$NUM
if [ "$dd" != "acquired" ] ; then
    echo ERROR ACQUIRING LOCK >&2
else
    echo dirlock acquired
fi
