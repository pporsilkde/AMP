#!/usr/bin/env python3
"""Arena updater worker. Standard library only; frozen into arena-updater.exe on Windows.
prepare REQUEST -> 0 unchanged/offline, 10 staged, 1 recoverable error, 20 unsafe to launch.
apply REQUEST waits for the launcher, commits a journalled transaction, then restarts it.
No shell commands or downloaded scripts are executed.
"""
import argparse
import contextlib
import ctypes
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import sys
import tarfile
import time
import urllib.parse
import urllib.request
import zipfile

CHUNK = 1024 * 1024
MAX_CHECK = 64 * 1024
MAX_EXPANDED = 128 * 1024**3
MAX_ENTRIES = 500000
PROTECTED_ROOTS = {'server', 'userdata', 'saves', 'screenshots', 'data files', 'datafiles', '.arena-update'}
PROTECTED_FILES = {'build.ini', 'check.ini', 'openmw.cfg', 'settings.cfg', 'launcher.cfg',
                   'tes3mp-client.cfg', 'tes3mp-server.cfg'}


def emit(phase, **values):
    print(json.dumps(dict(phase=phase, **values), ensure_ascii=True), flush=True)


def atomic_json(path, value):
    path = Path(path)
    tmp = path.with_name(path.name + '.tmp')
    with tmp.open('w', encoding='utf-8') as f:
        json.dump(value, f, ensure_ascii=True)
        f.flush()
        os.fsync(f.fileno())
    os.replace(str(tmp), str(path))


def read_ini(text):
    out = {}
    section = ''
    for raw in text.lstrip('\ufeff').splitlines():
        line = raw.strip()
        if not line or line.startswith(('#', ';')):
            continue
        if line.startswith('[') and line.endswith(']'):
            section = line[1:-1].strip().lower()
            continue
        if '=' in line:
            key, value = line.split('=', 1)
            value = value.strip()
            if len(value) >= 2 and value[0] == value[-1] == '"':
                value = value[1:-1]
                value = re.sub(r'\\([\\"nrt])', lambda m: {'n':'\n', 'r':'\r', 't':'\t'}.get(m[1], m[1]), value)
            out[(section, key.strip().lower())] = value
    return out


def build_value(values, key, default=''):
    for section in ('build', '', 'general', 'manifest'):
        if (section, key) in values:
            return values[(section, key)]
    return default


def revision(value):
    if not re.fullmatch(r'[0-9]{1,64}', value):
        raise ValueError('Invalid revision: ' + repr(value))
    return int(value, 10)


def stamp(text, versions):
    # Scalar-only edit: preserve comments, unknown sections and repeated content entries.
    lines = text.splitlines(keepends=True)
    section = ''
    found = set()
    for i, line in enumerate(lines):
        stripped = line.strip().lstrip('\ufeff')
        if stripped.startswith('[') and stripped.endswith(']'):
            section = stripped[1:-1].strip().lower()
        elif section in ('build', '', 'general', 'manifest') and '=' in stripped:
            key = stripped.split('=', 1)[0].strip().lower()
            if key in versions:
                lines[i] = key + '=' + versions[key] + '\n'
                found.add(key)
    if found != set(versions):
        lines.append('\n[Build]\n')
        lines.extend(k + '=' + v + '\n' for k, v in versions.items() if k not in found)
    return ''.join(lines)


def normalize_url(value):
    value = value.strip()
    if not value:
        raise ValueError('Update URL is empty')
    if '://' not in value:
        value = 'https://' + value
    parsed = urllib.parse.urlsplit(value)
    if parsed.scheme not in ('http', 'https') or not parsed.hostname or parsed.username or parsed.password:
        raise ValueError('Only HTTP(S) URLs without credentials are accepted')
    return value


class SafeRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        target = normalize_url(newurl)
        if req.full_url.startswith('https:') and not target.startswith('https:'):
            raise ValueError('HTTPS downgrade redirect refused')
        return super().redirect_request(req, fp, code, msg, headers, target)


def download(url, target=None, limit=None, expected_hash=''):
    opener = urllib.request.build_opener(SafeRedirect())
    req = urllib.request.Request(normalize_url(url), headers={'User-Agent':'ArenaMP-Updater/1', 'Cache-Control':'no-cache', 'Accept-Encoding':'identity'})
    timeout = 5 if limit else 30
    deadline = time.monotonic() + (8 if limit else 6 * 3600)
    with opener.open(req, timeout=timeout) as response:
        if response.status != 200:
            raise OSError('HTTP ' + str(response.status))
        total = int(response.headers.get('Content-Length', '-1'))
        if limit and total > limit:
            raise ValueError('check.ini is too large')
        digest = hashlib.sha256()
        data = bytearray()
        done = 0
        report_at = 0
        with contextlib.ExitStack() as stack:
            output = stack.enter_context(Path(target).open('wb')) if target else None
            while True:
                if time.monotonic() > deadline:
                    raise TimeoutError('Download timed out')
                block = response.read(4096 if limit else CHUNK)
                if not block:
                    break
                done += len(block)
                if limit and done > limit:
                    raise ValueError('check.ini is too large')
                digest.update(block)
                if output:
                    output.write(block)
                else:
                    data.extend(block)
                if target and time.monotonic() >= report_at:
                    emit('download', done=done, total=total)
                    report_at = time.monotonic() + 0.25
        if total >= 0 and total != done:
            raise ValueError('Incomplete download')
        if expected_hash:
            if not re.fullmatch(r'[0-9a-fA-F]{64}', expected_hash) or digest.hexdigest() != expected_hash.lower():
                raise ValueError('SHA-256 mismatch')
        return bytes(data)


def safe_name(name):
    name = name.replace('\\', '/')
    while name.startswith('./'):
        name = name[2:]
    if name in ('', '.'):
        return ''
    parts = name.rstrip('/').split('/')
    if name.startswith('/') or any(p in ('', '.', '..') for p in parts):
        raise ValueError('Unsafe archive path: ' + name)
    for part in parts:
        if '.arena-' in part or any(ord(c) < 32 or c in ':<>"|?*' for c in part) or part.endswith((' ', '.')):
            raise ValueError('Invalid archive path: ' + name)
        if part.split('.')[0].upper() in {'CON', 'PRN', 'AUX', 'NUL', *(f'COM{i}' for i in range(1, 10)), *(f'LPT{i}' for i in range(1, 10))}:
            raise ValueError('Reserved filename: ' + name)
    return '/'.join(parts)


def extract(archive, target):
    """Extract regular files only. TAR links become regular copies of internal file data.
    ZIP links/devices and TAR external/cyclic links are rejected before extraction.
    """
    target = Path(target)
    target.mkdir(parents=True, exist_ok=True)
    is_zip = zipfile.is_zipfile(archive)
    arc = zipfile.ZipFile(archive) if is_zip else tarfile.open(archive, 'r:*')
    with arc:
        members = arc.infolist() if is_zip else arc.getmembers()
        if len(members) > MAX_ENTRIES:
            raise ValueError('Too many archive entries')
        entries = {}
        folded = set()
        expanded = 0
        for m in members:
            name = safe_name(m.filename if is_zip else m.name)
            if not name:
                continue
            if name.casefold() in folded:
                raise ValueError('Duplicate/case-colliding archive path: ' + name)
            folded.add(name.casefold())
            if is_zip:
                mode = (m.external_attr >> 16) & 0xffff
                if stat.S_IFMT(mode) not in (0, stat.S_IFREG, stat.S_IFDIR):
                    raise ValueError('ZIP link/device refused: ' + name)
                if m.flag_bits & 1:
                    raise ValueError('Encrypted archive refused')
                directory, size = m.is_dir(), m.file_size
            else:
                if not (m.isdir() or m.isfile() or m.issym() or m.islnk()):
                    raise ValueError('TAR special file refused: ' + name)
                mode, directory, size = m.mode, m.isdir(), m.size
            entries[name] = (m, directory, mode)
            expanded += size
        # Validate parent paths and links before writing anything.
        for name, (m, directory, mode) in entries.items():
            for parent in PurePosixPath(name).parents:
                if str(parent) in entries and not entries[str(parent)][1]:
                    raise ValueError('File used as an archive directory')
        def resolve(name, visited=None):
            visited = set() if visited is None else visited
            if name in visited or name not in entries:
                raise ValueError('Dangling or cyclic TAR link: ' + name)
            visited.add(name)
            member, directory, mode = entries[name]
            if not is_zip and (member.issym() or member.islnk()):
                link = member.linkname.replace('\\', '/')
                if link.startswith('/') or ':' in link:
                    raise ValueError('External TAR link refused')
                base = PurePosixPath(name).parent if member.issym() else PurePosixPath('.')
                # Normalize relative link targets without allowing traversal beyond the archive root.
                parts = []
                for p in (base / link).parts:
                    if p == '..':
                        if not parts: raise ValueError('External TAR link refused')
                        parts.pop()
                    elif p != '.': parts.append(p)
                return resolve(safe_name('/'.join(parts)), visited)
            if directory: raise ValueError('Directory link refused')
            return member
        for name, (m, directory, mode) in entries.items():
            if not directory and not is_zip and (m.issym() or m.islnk()):
                expanded += resolve(name).size
        if expanded > MAX_EXPANDED or shutil.disk_usage(target).free < expanded + 16 * CHUNK:
            raise OSError('Not enough staging space, or archive exceeds 128 GiB')
        for name, (m, directory, mode) in entries.items():
            dest = target / name
            if directory:
                dest.mkdir(parents=True, exist_ok=True)
                continue
            dest.parent.mkdir(parents=True, exist_ok=True)
            source = arc.open(m) if is_zip else arc.extractfile(resolve(name))
            with source, dest.open('wb') as output:
                shutil.copyfileobj(source, output, CHUNK)
            os.chmod(dest, 0o755 if mode & 0o111 else 0o644)
        if not any(not v[1] for v in entries.values()):
            raise ValueError('Archive contains no files')


def payload_root(root, kind):
    root = Path(root)
    # Accept a single enclosing release directory. Content accepts a single Data Files wrapper.
    if kind == 'content':
        children = list(root.iterdir())
        if len(children) == 1 and children[0].is_dir() and children[0].name.casefold() == 'data files':
            return children[0]
        if any(p.name.casefold() == 'data files' for p in children):
            raise ValueError('Mixed content archive root; put only Data Files contents into update.zip')
        return root
    for _ in range(3):
        if any((root / f).is_file() for f in ('openmw-launcher.exe', 'openmw-launcher', 'openmw-launcher.x86_64')):
            return root
        children = list(root.iterdir())
        if len(children) != 1 or not children[0].is_dir():
            break
        root = children[0]
    raise ValueError('Engine archive must contain openmw-launcher at its root')


def protected(relative, kind):
    parts = PurePosixPath(relative).parts
    if kind == 'engine':
        return parts[0].casefold() in PROTECTED_ROOTS or Path(relative).name.casefold() in PROTECTED_FILES or parts[0].startswith('.arena-')
    if parts[0].casefold() in PROTECTED_FILES or parts[0].startswith('.arena-'):
        raise ValueError('Content archive may not contain client configuration: ' + relative)
    return False


def destination(root, relative):
    # Reject symlinks, including existing links in every parent directory. Do not follow them.
    root = Path(root).absolute()
    cur = root
    if root.is_symlink():
        raise ValueError('Update root is a symbolic link')
    parts = PurePosixPath(relative).parts
    for index, part in enumerate(parts):
        cur = cur / part
        if cur.is_symlink() and index != len(parts) - 1:
            raise ValueError('Destination symbolic link refused: ' + str(cur))
    return cur


def rollback(job):
    journal_path = Path(job) / 'journal.json'
    if not journal_path.exists(): return
    journal = json.loads(journal_path.read_text(encoding='utf-8'))
    if journal['state'] in ('done', 'rolled-back'): return
    for op in reversed(journal['operations']):
        dest, backup = Path(op['dest']), Path(op['backup'])
        if os.path.lexists(backup):
            os.replace(str(backup), str(dest))
        elif not op['original'] and op.get('started') and dest.exists():
            dest.unlink()
        Path(op['temp']).unlink(missing_ok=True)
    journal['state'] = 'rolled-back'
    atomic_json(journal_path, journal)


def commit(job, plan, fail_after=None):
    job = Path(job)
    journal_path = job / 'journal.json'
    journal = {'state':'applying', 'operations':[]}
    # Write-ahead intent for the ENTIRE transaction, once. Rewriting a growing
    # journal for every texture would cause quadratic disk traffic on large packs.
    for entry in plan['files']:
        dest = destination(entry['root'], entry['relative'])
        dest.parent.mkdir(parents=True, exist_ok=True)
        if os.path.lexists(dest) and not (dest.is_file() or dest.is_symlink()):
            raise ValueError('Destination is not a regular file: ' + str(dest))
        suffix = '.arena-' + job.name
        backup = dest.with_name(dest.name + suffix + '.bak')
        temp = dest.with_name(dest.name + suffix + '.tmp')
        if os.path.lexists(backup) or os.path.lexists(temp): raise ValueError('Stale transaction file')
        journal['operations'].append({'dest':str(dest), 'backup':str(backup), 'temp':str(temp),
                                      'original':os.path.lexists(dest), 'started':True})
    atomic_json(journal_path, journal)
    try:
        for index, (entry, op) in enumerate(zip(plan['files'], journal['operations'])):
            dest, backup, temp = Path(op['dest']), Path(op['backup']), Path(op['temp'])
            # Sibling temporary file guarantees atomic replace across cache/data disks.
            shutil.copyfile(entry['source'], temp)
            os.chmod(temp, stat.S_IMODE(Path(entry['source']).stat().st_mode))
            # Windows rejects fsync() on a read-only descriptor (errno 9).
            # Open read/write so the same durability step works on Windows and POSIX.
            with temp.open('r+b') as f: os.fsync(f.fileno())
            if op['original']: os.replace(str(dest), str(backup))
            os.replace(str(temp), str(dest))
            if fail_after is not None and index == fail_after: raise OSError('Injected commit failure')
        journal['state'] = 'done'
        atomic_json(journal_path, journal)
    except BaseException:
        rollback(job)
        raise
    for op in journal['operations']:
        try: Path(op['backup']).unlink(missing_ok=True)
        except OSError: pass


def alive(pid):
    if not pid or pid <= 0: return False
    if os.name == 'nt':
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.OpenProcess.restype = ctypes.c_void_p
        kernel.OpenProcess.argtypes = [ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
        handle = kernel.OpenProcess(0x100000, False, pid)
        if not handle: return ctypes.get_last_error() == 5
        try: return kernel.WaitForSingleObject(handle, 0) == 258
        finally: kernel.CloseHandle(handle)
    try:
        os.kill(pid, 0)
        # On Linux an exited launcher can remain as a zombie until the
        # desktop/session parent reaps it.  kill(pid, 0) still succeeds for a
        # zombie, which would make apply wait the full 120-second deadline and
        # report that the launcher never closed.  Treat the zombie state as
        # exited so the staged client can be committed immediately.
        if sys.platform.startswith('linux'):
            stat_path = Path('/proc') / str(pid) / 'stat'
            try:
                stat_text = stat_path.read_text(encoding='utf-8')
                close_paren = stat_text.rfind(')')
                if close_paren >= 0 and stat_text[close_paren + 2:close_paren + 3] == 'Z':
                    return False
            except (OSError, UnicodeError):
                pass
        return True
    except ProcessLookupError: return False
    except PermissionError: return True


def other_clients(request):
    root = Path(request['client']).resolve()
    excluded = {os.getpid(), request['parent_pid']}
    found = []
    if sys.platform.startswith('linux'):
        for proc in Path('/proc').iterdir():
            if not proc.name.isdigit() or int(proc.name) in excluded: continue
            try:
                exe = (proc / 'exe').resolve(strict=True)
                if exe.parent == root and exe.name.startswith(('tes3mp', 'openmw')):
                    found.append(exe.name)
            except (OSError, RuntimeError): pass
    elif os.name == 'nt':
        from ctypes import wintypes
        class ProcessEntry(ctypes.Structure):
            _fields_ = [('size', wintypes.DWORD), ('usage', wintypes.DWORD), ('pid', wintypes.DWORD),
                        ('heap', ctypes.c_size_t), ('module', wintypes.DWORD), ('threads', wintypes.DWORD),
                        ('parent', wintypes.DWORD), ('priority', wintypes.LONG), ('flags', wintypes.DWORD),
                        ('name', wintypes.WCHAR * 260)]
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
        kernel.OpenProcess.restype = wintypes.HANDLE
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        kernel.Process32FirstW.argtypes = [wintypes.HANDLE, ctypes.POINTER(ProcessEntry)]
        kernel.Process32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(ProcessEntry)]
        kernel.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
        snapshot = kernel.CreateToolhelp32Snapshot(2, 0)
        if snapshot == ctypes.c_void_p(-1).value: raise OSError('Cannot inspect running clients')
        try:
            entry = ProcessEntry(); entry.size = ctypes.sizeof(entry)
            more = kernel.Process32FirstW(snapshot, ctypes.byref(entry))
            while more:
                if entry.pid not in excluded and entry.name.lower().startswith(('tes3mp', 'openmw')):
                    handle = kernel.OpenProcess(0x1000, False, entry.pid)
                    if handle:
                        try:
                            size = wintypes.DWORD(32768); path = ctypes.create_unicode_buffer(size.value)
                            if kernel.QueryFullProcessImageNameW(handle, 0, path, ctypes.byref(size)):
                                if Path(path.value).resolve().parent == root: found.append(entry.name)
                        finally: kernel.CloseHandle(handle)
                more = kernel.Process32NextW(snapshot, ctypes.byref(entry))
        finally: kernel.CloseHandle(snapshot)
    return found


@contextlib.contextmanager
def installation_lock(manifest):
    lock = Path(manifest).parent / '.arena-update.lock'
    f = lock.open('a+b')
    try:
        if os.name == 'nt':
            import msvcrt
            f.seek(0); f.write(b'0'); f.flush(); f.seek(0)
            msvcrt.locking(f.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(f.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        yield
    finally:
        f.close()


def recover_pending(request):
    pointer = Path(request['manifest']).parent / '.arena-update-pending.json'
    if not pointer.exists(): return
    pending = json.loads(pointer.read_text(encoding='utf-8'))
    if alive(pending.get('owner', 0)):
        raise RuntimeError('Another launcher/updater owns a pending update')
    job = Path(pending['job'])
    rollback(job)
    pointer.unlink()


def prepare(request, job):
    manifest = Path(request['manifest'])
    with installation_lock(manifest):
        try: recover_pending(request)
        except Exception as exc:
            emit('blocked', message=str(exc)); return 20
        local = read_ini(manifest.read_text(encoding='utf-8-sig'))
        check_url = build_value(local, 'url_check')
        if not check_url: return 0
        try:
            emit('check')
            remote = read_ini(download(check_url, limit=MAX_CHECK).decode('utf-8-sig'))
            requested = {}
            for key in ('version', 'build'):
                value = build_value(remote, key)
                if revision(value) > revision(build_value(local, key, '00000')):
                    requested[key] = value
        except Exception as exc:
            # Offline/HTTP/invalid check is deliberately non-blocking.
            emit('offline', message=str(exc)); return 0
        if not requested: return 0
        if request['engine_key'] == 'url_macos' and not build_value(local, 'url_macos'):
            requested.pop('build', None)
        if not requested: return 0
        running = other_clients(request)
        if running:
            emit('blocked', message='Close the running game/server/wizard before updating: ' + ', '.join(running))
            return 20
        plan = {'files':[], 'versions':requested}
        destinations = set()
        targets = {}
        if 'version' in requested:
            targets['content'] = (request['data'], build_value(local, 'url_update') or build_value(local, 'update'), 'sha256_update')
        if 'build' in requested:
            key = request['engine_key']
            targets['engine'] = (request['client'], build_value(local, key), 'sha256_' + key[4:])
        for kind, (target, url, hash_key) in targets.items():
            emit('package', kind=kind)
            archive = job / (kind + '.download')
            download(url, archive, expected_hash=build_value(remote, hash_key))
            stage = job / kind
            emit('extract', kind=kind)
            extract(archive, stage)
            archive.unlink()
            payload = payload_root(stage, kind)
            if kind == 'engine':
                expected = ('tes3mp.exe',) if request['engine_key'] == 'url_win' else ('tes3mp', 'tes3mp.x86_64')
                if not any((payload / name).is_file() for name in expected):
                    raise ValueError('Engine archive does not contain this platform client')
            for source in sorted(payload.rglob('*')):
                if not source.is_file(): continue
                relative = source.relative_to(payload).as_posix()
                if protected(relative, kind): continue
                dest = destination(target, relative)
                # Case-insensitive path collisions across both packages are rejected.
                if str(dest).casefold() in destinations:
                    raise ValueError('Overlapping update destinations')
                destinations.add(str(dest).casefold())
                plan['files'].append({'source':str(source), 'root':str(Path(target).absolute()), 'relative':relative})
        if not plan['files']: raise ValueError('No applicable update files')
        # The manifest is the FINAL operation in the same transaction as both packages.
        version_file = job / 'build.ini'
        version_file.write_text(stamp(manifest.read_text(encoding='utf-8-sig'), requested), encoding='utf-8')
        plan['files'].append({'source':str(version_file), 'root':str(manifest.parent), 'relative':manifest.name})
        atomic_json(job / 'plan.json', plan)
        atomic_json(manifest.parent / '.arena-update-pending.json', {'job':str(job), 'owner':request['parent_pid']})
        emit('ready', files=len(plan['files']))
        return 10


def apply(request, job):
    result_path = Path(request['manifest']).parent / '.arena-update-result.json'
    pointer = Path(request['manifest']).parent / '.arena-update-pending.json'
    deadline = time.monotonic() + 120
    while alive(request['parent_pid']):
        if time.monotonic() > deadline:
            atomic_json(result_path, {'ok':False, 'message':'Launcher did not exit; update was not applied'})
            return 1
        time.sleep(0.2)
    ok = False
    try:
        with installation_lock(request['manifest']):
            atomic_json(pointer, {'job':str(job), 'owner':os.getpid()})
            if other_clients(request): raise RuntimeError('A game/server/wizard is still running; no files changed')
            plan = json.loads((job / 'plan.json').read_text(encoding='utf-8'))
            commit(job, plan)
            pointer.unlink(missing_ok=True)
            ok = True
            atomic_json(result_path, {'ok':True, 'versions':plan['versions']})
    except Exception as exc:
        atomic_json(result_path, {'ok':False, 'message':str(exc)})
    # Failed updates reopen the launcher, but never auto-connect.
    args = [request['launcher']] + request.get('launcher_args', [])
    if ok: args.append('--arena-update-resume')
    try:
        env = os.environ.copy()
        if getattr(sys, 'frozen', False):
            if os.name == 'nt': ctypes.windll.kernel32.SetDllDirectoryW(None)
            elif 'LD_LIBRARY_PATH_ORIG' in env: env['LD_LIBRARY_PATH'] = env['LD_LIBRARY_PATH_ORIG']
            else: env.pop('LD_LIBRARY_PATH', None)
        subprocess.Popen(args, cwd=request['client'], close_fds=True, env=env,
                         creationflags=0x00000008 if os.name == 'nt' else 0,
                         start_new_session=os.name != 'nt')
    except Exception as exc:
        atomic_json(result_path, {'ok':False, 'message':'Update applied; reopen launcher manually. ' + str(exc)})
        return 1
    if ok:
        # The helper may itself live in job; Windows cannot delete a running frozen exe.
        shutil.rmtree(job, ignore_errors=True)
    return 0 if ok else 1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=('prepare', 'apply'))
    parser.add_argument('request')
    args = parser.parse_args()
    path = Path(args.request).absolute()
    request = json.loads(path.read_text(encoding='utf-8'))
    try:
        return prepare(request, path.parent) if args.action == 'prepare' else apply(request, path.parent)
    except Exception as exc:
        emit('error', message=str(exc))
        return 1


if __name__ == '__main__':
    sys.exit(main())
