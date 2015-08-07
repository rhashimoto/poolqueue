#!/bin/bash

if type mpiexec && [ -f ./MPI_test ]; then
    mpiexec -n 1 ./MPI_test
fi

   
