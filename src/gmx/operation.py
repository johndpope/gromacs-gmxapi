"""
Python front-end to gmxapi work graph Operations
================================================

Reference https://github.com/kassonlab/gmxapi/issues/208

Consider an example work graph like the following.

    subgraph = gmx.subgraph(input={'conformation': initial_input})

    # As an alternative to specifying the context or graph in each call, intuiting the graph,
    # or requiring the user to manage globally switching, we could use a context manager.
    with subgraph:
        modified_input = gmx.modify_input(input=initial_input, structure=subgraph.input.conformation)
        md = gmx.mdrun(input=modified_input)
        # Assume the existence of a more integrated gmx.trajcat operation
        cluster = gmx.command_line('gmx', 'cluster', input=gmx.reduce(gmx.trajcat, md.output.trajectory))
        condition = mymodule.cluster_analyzer(input=cluster.output.file["-ev"])
        subgraph.output.conformation = cluster.output.conformation

    # In the default work graph, add a node that depends on `condition` wraps subgraph.
    # It makes sense that a convergence-checking operation is initialized such that
    # `is_converged() == False`
    my_loop = gmx.while(gmx.logical_not(condition.output.is_converged), subgraph)
    gmx.run()

What is produced by mymodule.cluster_analyzer?


"""
# The object returned needs to be usable in the definition of a work graph, as well
# as in the running of it.
#
# Possibly the easiest way, and the way that would be most
# consistent with what we've done so far, would be for the `mymodule` Python module
# (optionally "package") to have a `cluster_analyzer` function with multiple
#     signatures. A signature that takes a single WorkElement object produces a Director
# for the operation at launch time. This would be compatible with the 0.0.7 spec.
#
# To allow the flexible usage described above, the object would need to have `input`
# and `output` properties that each provide properties that look like named gmxapi
# input or output data. Also see gist: https://gist.github.com/eirrgang/0d975eb279fce21f59aa29de2b1316f2
#
# Side note: I'm not sure the objects need to have separate `input` and `output`
# properties. We don't really have a use case yet for assigning to `input` without
# using keyword arguments in a function, so maybe there is no `input` property and
# the `output` categorization is implicit for Operation properties.

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals


def make_operation():
    """Wrap a class to produce an object that acts as a gmxapi Operation."""
    return None
