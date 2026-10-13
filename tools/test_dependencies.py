"""Check download integrity, offline reuse, safe extraction and cache replacement."""
import hashlib
import io
import tarfile
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from setup_dependencies import install


def fixture(extra=None):
    output = io.BytesIO()
    files = {'library.properties': b'name=example\n', 'LICENSE': b'license\n',
             'example.h': b'// pinned source\n'}
    if extra:
        files.update(extra)
    with tarfile.open(fileobj=output, mode='w:gz') as archive:
        for name, data in files.items():
            member = tarfile.TarInfo('example-revision/' + name)
            member.size = len(data)
            archive.addfile(member, io.BytesIO(data))
    data = output.getvalue()
    dependency = {'revision': 'a' * 40, 'archive_url': 'https://example.invalid/source.tar.gz',
                  'sha256': hashlib.sha256(data).hexdigest()}
    return data, dependency


class DependencyTest(unittest.TestCase):
    def test_install_and_offline_reuse(self):
        data, dependency = fixture()
        with tempfile.TemporaryDirectory() as temporary:
            cache = Path(temporary)
            with patch('setup_dependencies.urllib.request.urlopen', return_value=io.BytesIO(data)):
                target = install('example', dependency, cache)
            self.assertEqual((target / 'example.h').read_bytes(), b'// pinned source\n')
            with patch('setup_dependencies.urllib.request.urlopen') as download:
                self.assertEqual(install('example', dependency, cache), target)
                download.assert_not_called()

    def test_corrupt_download_preserves_existing_cache(self):
        data, dependency = fixture()
        with tempfile.TemporaryDirectory() as temporary:
            cache = Path(temporary)
            with patch('setup_dependencies.urllib.request.urlopen', return_value=io.BytesIO(data)):
                target = install('example', dependency, cache)
            with patch('setup_dependencies.urllib.request.urlopen', return_value=io.BytesIO(b'corrupt')):
                with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
                    install('example', dependency, cache, force=True)
            self.assertEqual((target / 'example.h').read_bytes(), b'// pinned source\n')

    def test_modified_or_added_sources_require_explicit_refresh(self):
        data, dependency = fixture()
        with tempfile.TemporaryDirectory() as temporary:
            cache = Path(temporary)
            with patch('setup_dependencies.urllib.request.urlopen', return_value=io.BytesIO(data)):
                target = install('example', dependency, cache)
            for name in ['example.h', 'added.cpp']:
                (target / name).write_text('// locally changed\n')
                with self.assertRaisesRegex(ValueError, 'cache differs'):
                    install('example', dependency, cache)
                with patch('setup_dependencies.urllib.request.urlopen', return_value=io.BytesIO(data)):
                    install('example', dependency, cache, force=True)
            self.assertFalse((target / 'added.cpp').exists())

    def test_traversal_archive_is_rejected_before_install(self):
        data, dependency = fixture({'../escape': b'not allowed'})
        with tempfile.TemporaryDirectory() as temporary:
            cache = Path(temporary) / 'cache'
            with patch('setup_dependencies.urllib.request.urlopen', return_value=io.BytesIO(data)):
                with self.assertRaisesRegex(ValueError, 'Invalid archive path'):
                    install('example', dependency, cache)
            self.assertFalse((cache / 'example').exists())
            self.assertFalse((Path(temporary) / 'escape').exists())


if __name__ == '__main__':
    unittest.main()
