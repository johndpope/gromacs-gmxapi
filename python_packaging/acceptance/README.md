# Acceptance testing

`../roadmap.rst` summarizes the expected functionality to deliver during 2019 Q2. `RequiredFunctionality
.ipynb` focuses on functionality and behavior related 
to the Python package.

Targeted behavior can be explored and verified interactively.
For an Jupyter notebook build the `../docker/acceptance.dockerfile` image,
or
1. build and install GROMACS with GMXAPI=ON
2. source the GMXRC
3. install the `gmxapi` Python package
4. run a Jupyter notebook server and navigate to RequiredFunctionality.ipynb in this directory.
