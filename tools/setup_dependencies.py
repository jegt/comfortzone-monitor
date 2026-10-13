#!/usr/bin/env python3
"""Download checksum-locked Arduino dependencies into an ignored local cache."""
import argparse
import hashlib
import io
import json
import re
import tarfile
import tempfile
import urllib.request
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[1]
CACHE = ROOT / '.dependencies'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def cached(target, dependency):
    try:
        marker = json.loads((target / '.dependency.json').read_text())
        if marker['dependency'] != dependency or not marker['files']:
            return False
        paths = [path for path in target.rglob('*') if path.is_file()]
        if target.is_symlink() or any(path.is_symlink() for path in paths):
            return False
        actual = {path.relative_to(target).as_posix() for path in paths
                  if path.name != '.dependency.json'}
        if actual != set(marker['files']):
            return False
        return all(digest((target / name).read_bytes()) == expected
                   for name, expected in marker['files'].items())
    except (OSError, KeyError, ValueError):
        return False


def install(name, dependency, cache=CACHE, force=False):
    if not re.fullmatch(r'[A-Za-z][A-Za-z0-9_-]*', name):
        raise ValueError('Invalid dependency name')
    if not re.fullmatch(r'[0-9a-f]{40}', dependency['revision']):
        raise ValueError('Dependency must be pinned to a full Git revision')
    if not re.fullmatch(r'[0-9a-f]{64}', dependency['sha256']):
        raise ValueError('Dependency must have an archive SHA-256')
    target = cache / name
    if target.exists() and not force:
        if cached(target, dependency):
            print(f'{name}: verified cached revision {dependency["revision"][:12]}')
            return target
        raise ValueError(f'{name}: cache differs from the lock; run tools/setup_dependencies.py --force to replace it')

    print(f'{name}: downloading revision {dependency["revision"][:12]}')
    with urllib.request.urlopen(dependency['archive_url'], timeout=30) as response:
        data = response.read()
    if digest(data) != dependency['sha256']:
        raise ValueError(f'{name}: archive checksum mismatch; dependency was not installed')

    cache.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.setup-', dir=cache) as temporary:
        stage = Path(temporary) / name
        stage.mkdir()
        files = {}
        archive_root = None
        with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as archive:
            for member in archive:
                path = PurePosixPath(member.name)
                if path.is_absolute() or '..' in path.parts or not path.parts:
                    raise ValueError('Invalid archive path')
                if archive_root is None:
                    archive_root = path.parts[0]
                if path.parts[0] != archive_root:
                    raise ValueError('Archive must have one root directory')
                if member.isdir():
                    continue
                if not member.isfile() or len(path.parts) < 2:
                    raise ValueError('Only regular dependency files are allowed')
                relative = Path(*path.parts[1:])
                if relative.as_posix() in files or relative.name == '.dependency.json':
                    raise ValueError('Duplicate or reserved archive path')
                contents = archive.extractfile(member).read()
                destination = stage / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(contents)
                files[relative.as_posix()] = digest(contents)
        if not all(name in files for name in ['library.properties', 'LICENSE']):
            raise ValueError('Archive is missing library metadata or license')
        (stage / '.dependency.json').write_text(json.dumps(
            {'dependency': dependency, 'files': files}, indent=2) + '\n')
        backup = Path(temporary) / 'previous'
        if target.exists():
            target.rename(backup)
        try:
            stage.rename(target)
        except OSError:
            if backup.exists():
                backup.rename(target)
            raise
    print(f'{name}: installed and checksum verified')
    return target


def ensure_dependencies(force=False):
    dependencies = json.loads((ROOT / 'dependencies.json').read_text())
    for name, dependency in dependencies.items():
        install(name, dependency, force=force)
    return CACHE


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--force', action='store_true', help='Replace an existing cache after verifying the download')
    arguments = parser.parse_args()
    try:
        ensure_dependencies(force=arguments.force)
    except (OSError, ValueError, KeyError, tarfile.TarError) as error:
        raise SystemExit(str(error))
