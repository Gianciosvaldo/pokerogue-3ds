#!/usr/bin/env python3
"""Native closure identities, real helper commands and actual C registry accounting."""
from pathlib import Path
from collections import defaultdict
import gzip
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
import unittest

CORE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CORE / 'gen'))
from closure_identity import closure_hash


def command(args):
    p = subprocess.run(list(map(str, args)), cwd=CORE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if p.returncode:
        raise RuntimeError(str(args) + '\n' + p.stdout[-4000:])
    return p.stdout


class NativeClosureIdentity(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        capsule = json.loads(gzip.decompress((CORE / 'tests/fixtures/closure-identity.native.json.gz').read_bytes()))
        assert capsule['source_pin'] == '8555c08c823b856cbec4eb99ca84ea52a955836d'
        assert capsule['locale_pin'] == '270ed0a2938b2c3e2e34e7de940e84aecadf548b'
        assert capsule['asset_pin'] == '87426a79611f9d212c4dc8af58e2834a05b93725'
        assert capsule['producer_sha256'] == hashlib.sha256((CORE / 'gen/dump-upstream.test.ts').read_bytes()).hexdigest()
        cls.files = capsule['files']
        cls.closures = cls.files['closures.json']
        cls.manifest = [line.split() for line in (CORE / 'gen/out/closures.txt').read_text().splitlines() if line and not line.startswith('#')]
        assert len(cls.closures) == len(cls.manifest) == 735
        cls.temp = tempfile.TemporaryDirectory(prefix='closure-identity-')
        cls.folder = Path(cls.temp.name)
        for name, data in cls.files.items():
            (cls.folder / name).write_text(json.dumps(data))

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def helper(self, *args):
        return command([sys.executable, CORE / 'tools/closures.py', '--dump-dir', self.folder, *args])

    def test_all_native_identities_match_checked_in_generator_manifest(self):
        for closure, (id_, owner, expected) in zip(self.closures, self.manifest):
            with self.subTest(owner=owner, id=id_):
                self.assertEqual((closure['id'], closure['owner']), (int(id_), owner))
                self.assertEqual(closure_hash(closure), expected)

    def test_actual_generator_preserves_all_checked_in_output_bytes(self):
        output = self.folder / 'generated'
        output.mkdir()
        env = dict(os.environ, PR_BATTLE_DUMP_DIR=str(self.folder), PR_BATTLE_GENERATED_DIR=str(output))
        p = subprocess.run([sys.executable, str(CORE / 'gen/gen_tables.py')], cwd=CORE, env=env,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        self.assertEqual(p.returncode, 0, p.stdout)
        self.assertEqual({f.name for f in output.iterdir()}, {'closures.txt', 'pr_attrs.h', 'pr_data.c', 'pr_data.h', 'pr_enums.h'})
        for file in output.iterdir():
            self.assertEqual(file.read_bytes(), (CORE / 'gen/out' / file.name).read_bytes(), file.name)

    def test_show_preserves_all_captured_numeric_values(self):
        expected = {90: '8cbec46369f4', 93: '60455f51213a', 158: '8cbec46369f4', 247: '5141b1d3bea9'}
        captured = [c for c in self.closures if 'vals' in c]
        self.assertEqual({c['id'] for c in captured}, set(expected))
        for c in captured:
            shown = self.helper('show', c['owner'])
            self.assertIn(expected[c['id']], shown)
            self.assertIn('vals=' + str(c['vals']), shown)
            changed = dict(c, vals=[c['vals'][0] + 1, *c['vals'][1:]])
            self.assertNotEqual(closure_hash(changed), expected[c['id']])

    def test_fill_all_native_owner_positions_with_manifest_hashes(self):
        counts = defaultdict(int)
        placeholders, expected = [], []
        for c, (_, owner, digest) in zip(self.closures, self.manifest):
            position = counts[owner]
            counts[owner] += 1
            placeholders.append('{"' + owner + '", "?' + str(position) + '", implementation},\n')
            expected.append('{"' + owner + '", "' + digest + '", implementation},\n')
        target = self.folder / 'registry.c'
        target.write_text(''.join(placeholders))
        self.helper('fill', target)
        self.assertEqual(target.read_text(), ''.join(expected))

    def test_status_agrees_with_real_c_registry_and_avoids_coverage_claim(self):
        command(['make', '-s', '-C', CORE, 'build/libprcore.a'])
        source, exe = self.folder / 'count.c', self.folder / 'count'
        source.write_text('#include "pr/battle.h"\n#include <stdio.h>\nint main(void){printf("%d\\n",pr_closures_ported());return 0;}\n')
        command([os.environ.get('CC', 'cc'), '-I' + str(CORE / 'include'), '-I' + str(CORE / 'gen/out'), source, CORE / 'build/libprcore.a', '-lm', '-o', exe])
        actual = int(command([exe]).strip())
        shown = self.helper('status')
        self.assertEqual(int(re.match(r'(\d+) of 735 closures', shown).group(1)), actual)
        self.assertIn('generic registry/builtin', shown)
        self.assertIn('class-specific callers require separate review', shown)
        self.assertIn('not semantic coverage', shown)


if __name__ == '__main__':
    unittest.main()
