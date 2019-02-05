#!/bin/bash
set -e

# Set up environment for "testing" user here.
source $HOME/testing/bin/activate

exec "$@"