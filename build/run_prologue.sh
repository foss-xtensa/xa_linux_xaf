#!/bin/bash

set -x

function on_exit
{
  if kill -0 ${pids[0]} > /dev/null 2>&1; then
    kill -9 ${pids[0]}
  fi
  rm -f /dev/shm/S*RAM_L.${pids[0]}
  rm -f /dev/shm/L2
}

trap on_exit EXIT
trap on_exit INT

