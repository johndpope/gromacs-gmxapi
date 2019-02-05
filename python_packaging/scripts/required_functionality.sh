#!/bin/bash

cd $HOME/acceptance
jupyter nbconvert RequiredFunctionality.ipynb --to python
python RequiredFunctionality.py
# or
# mpiexec -n 2 python -m mpi4py RequiredFunctionality.py