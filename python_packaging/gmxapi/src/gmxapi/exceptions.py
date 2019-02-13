# This file is part of the GROMACS molecular simulation package.
#
# Copyright (c) 2019, by the GROMACS development team, led by
# Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
# and including many others, as listed in the AUTHORS file in the
# top-level source directory and at http://www.gromacs.org.
#
# GROMACS is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2.1
# of the License, or (at your option) any later version.
#
# GROMACS is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with GROMACS; if not, see
# http://www.gnu.org/licenses, or write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
#
# If you want to redistribute modifications to GROMACS, please
# consider that scientific software is very special. Version
# control is crucial - bugs must be traceable. We will be happy to
# consider code for inclusion in the official distribution, but
# derived work must not be called official GROMACS. Details are found
# in the README & COPYING files - if they are missing, get the
# official version at http://www.gromacs.org.
#
# To help us fund GROMACS development, we humbly ask that you cite
# the research papers on the package. Check out http://www.gromacs.org.

"""
Exceptions and Warnings raised by gmx module operations
=======================================================

Errors, warnings, and other exceptions used in the Gromacs
gmx Python package are defined in the gmx.exceptions submodule.

The Gromacs gmx Python package defines a root exception,
gmx.exceptions.Error, from which all Exceptions thrown from
within the module should derive. If a published component of
the gmx package throws an exception that cannot be caught
as a gmx.exceptions.Error, please report the bug.
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals


__all__ = ['Error',
           'ApiError',
           'AttributeError',
           'CompatibilityError',
           'FeatureNotAvailableError',
           'FeatureNotAvailableWarning',
           'FileError',
           'TypeError',
           'UsageError',
           'ValueError',
           ]

class Error(Exception):
    """Base exception for gmx.exceptions classes."""

class Warning(Warning):
    """Base warning class for gmx.exceptions."""

class UsageError(Error):
    """Unsupported syntax or call signatures.

    Generic usage error for Gromacs gmx module.
    """

class ApiError(Error):
    """An API operation was attempted with an incompatible object."""

class AttributeError(ApiError):
    """Attribute of a gmxapi module, class, or object is not accessible."""

class CompatibilityError(Error):
    """An operation or data is incompatible with the current gmxapi environment."""

class FileError(Error):
    """Problem with a file or filename."""

class FeatureNotAvailableError(Error):
    """Feature is not installed, is missing dependencies, or is not compatible."""

class FeatureNotAvailableWarning(Warning):
    """Feature is not installed, is missing dependencies, or is not compatible."""

class TypeError(Error):
    """An object is of a type incompatible with the API operation."""
    def __init__(self, got=None, expected=None):
        message = "Incompatible type."
        if expected is not None:
            message += " Expected type {}.".format(expected)
        if got is not None:
            message += " Got type {}.".format(type(got))
        super(TypeError, self).__init__(message)

class ValueError(Error):
    """A user-provided value cannot be interpreted or doesn't make sense."""
