"""Identity used by generated tables and the closure inspection/fill helper."""
import hashlib
import json


def closure_hash(cl):
    """Source text plus all probed captured arguments, preserving dump order."""
    key = cl['src'] + ('\n#cap ' + json.dumps(cl['cap']) if 'cap' in cl else '')
    key += '\n#vals ' + json.dumps(cl['vals']) if 'vals' in cl else ''
    return hashlib.sha1(key.encode()).hexdigest()[:12]
