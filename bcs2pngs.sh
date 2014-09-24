#!/bin/bash
for file in *.bc
do
	bc2pngs ${file}
    rc=$?
    if [[ $rc != 0 ]] ; then
        exit $rc
    fi
done
