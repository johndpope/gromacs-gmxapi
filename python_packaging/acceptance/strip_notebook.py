#!/usr/bin/env python
# Strip artifacts of execution from Jupyter notebook, such as before committing updates with git.
# It seems like `jupyter nbconvert --clear-output --to notebook` should do this, but it doesn't seem to...

import json
import sys
import tempfile
import os
from warnings import warn

filename = sys.argv[1]

with open(filename, 'r') as fh:
    nb_contents = json.load(fh)

for cell in nb_contents['cells']:
    if cell['cell_type'] == 'code':
        cell['execution_count'] = None
        cell['outputs'] = []

try:
    temporary = tempfile.NamedTemporaryFile(mode='w', delete=False)
    json.dump(nb_contents, temporary, indent=1)
    temporary.close()
    os.rename(temporary.name, filename)
except:
    warn('Errors occurred and temporary file may have been left behind.')
    warn('Check for file named {} and manually remove.'.format(temporary.name))
