import functools
import http.server
import importlib.util
import io
import json
import os
from pathlib import Path
import tarfile
import tempfile
import threading
import unittest
import zipfile

spec = importlib.util.spec_from_file_location('updater', Path(__file__).with_name('arena_updater.py'))
u = importlib.util.module_from_spec(spec); spec.loader.exec_module(u)

class UpdaterTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.root = Path(self.tmp.name)
    def tearDown(self): self.tmp.cleanup()
    def zip(self, entries, name='update.zip'):
        p = self.root / name
        with zipfile.ZipFile(p, 'w', zipfile.ZIP_DEFLATED) as z:
            for n, data in entries.items(): z.writestr(n, data)
        return p
    def test_numeric_revisions_and_preserving_manifest(self):
        self.assertGreater(u.revision('00010'), u.revision('00009'))
        self.assertEqual(u.revision('00001'), u.revision('1'))
        for bad in ('-1', '1.2', '', 'nan', '1'*65):
            with self.assertRaises(ValueError): u.revision(bad)
        text = '# comment\n[Build]\nformat=1\nversion=00001\nbuild=00002\nurl="t.me/arena_mp"\n[Server]\naddress="178.20.47.31"\n[Content]\ncontent=A.esm\ncontent=B.esm\n'
        changed = u.stamp(text, {'version':'00003'})
        self.assertEqual(changed, text.replace('version=00001','version=00003'))
        self.assertEqual(u.build_value(u.read_ini('\ufeffversion=00002\nbuild=00001'), 'version'), '00002')
    def test_extract_root_and_wrapper(self):
        for prefix in ('', 'Data Files/'):
            with self.subTest(prefix=prefix):
                stage = self.root / ('stage'+str(len(prefix)))
                u.extract(self.zip({prefix+'Textures/test.dds':b'texture',prefix+'test.esm':b'plugin'}), stage)
                root = u.payload_root(stage,'content')
                self.assertEqual((root/'Textures/test.dds').read_bytes(),b'texture')
    def test_zip_paths_and_case_collisions(self):
        for entries in ({'../evil':b'x'}, {'/evil':b'x'}, {'C:/evil':b'x'}, {'x/../../evil':b'x'},
                        {'x:stream':b'x'}, {'Textures/A':b'x','textures/a':b'y'}, {'parent':b'x','parent/child':b'y'}):
            with self.subTest(entries=entries):
                with tempfile.TemporaryDirectory(dir=self.root) as d:
                    with self.assertRaises((ValueError, OSError)): u.extract(self.zip(entries),d)
        self.assertFalse((self.root.parent/'evil').exists())
    def test_zip_symlink_refused(self):
        p = self.root/'links.zip'
        with zipfile.ZipFile(p,'w') as z:
            entry=zipfile.ZipInfo('escape');entry.create_system=3;entry.external_attr=0o120777<<16
            z.writestr(entry,'../../outside')
        with self.assertRaises(ValueError): u.extract(p,self.root/'stage')
    @unittest.skipIf(os.name == "nt", "Windows does not expose POSIX executable mode bits")
    def test_tar_internal_links_and_executable(self):
        p=self.root/'engine.tar.gz'
        with tarfile.open(p,'w:gz') as tar:
            info=tarfile.TarInfo('./lib/liba.so.1');info.size=4;info.mode=0o755;tar.addfile(info,io.BytesIO(b'ELF!'))
            link=tarfile.TarInfo('./lib/liba.so');link.type=tarfile.SYMTYPE;link.linkname='liba.so.1';tar.addfile(link)
        stage=self.root/'stage';u.extract(p,stage)
        self.assertEqual((stage/'lib/liba.so').read_bytes(),b'ELF!')
        self.assertFalse((stage/'lib/liba.so').is_symlink())
        self.assertTrue((stage/'lib/liba.so.1').stat().st_mode & 0o111)
    def test_tar_external_link_refused(self):
        p=self.root/'bad.tar.gz'
        with tarfile.open(p,'w:gz') as tar:
            link=tarfile.TarInfo('bad');link.type=tarfile.SYMTYPE;link.linkname='../../etc/passwd';tar.addfile(link)
        with self.assertRaises(ValueError): u.extract(p,self.root/'stage')
    def plan(self):
        target=self.root/'target';target.mkdir();job=self.root/'job';job.mkdir()
        (target/'old').write_text('old');(target/'build.ini').write_text('version=00001\nbuild=00001\n')
        plan={'files':[]}
        for name,text in [('old','new'),('newfile','new'),('build.ini','version=00002\nbuild=00001\n')]:
            source=job/name;source.write_text(text)
            plan['files'].append({'source':str(source),'root':str(target),'relative':name})
        return target,job,plan
    def test_commit_and_version_last(self):
        target,job,plan=self.plan();u.commit(job,plan)
        self.assertEqual((target/'old').read_text(),'new')
        self.assertIn('00002',(target/'build.ini').read_text())
        self.assertFalse(list(target.glob('*.bak')))
    def test_commit_failure_restores_new_and_existing_files(self):
        target,job,plan=self.plan()
        with self.assertRaises(OSError): u.commit(job,plan,fail_after=1)
        self.assertEqual((target/'old').read_text(),'old')
        self.assertFalse((target/'newfile').exists())
        self.assertIn('00001',(target/'build.ini').read_text())
    def test_crash_recovery_is_idempotent(self):
        target,job,plan=self.plan()
        dest=target/'old';backup=target/'old.bak';dest.rename(backup);dest.write_text('partial')
        u.atomic_json(job/'journal.json',{'state':'applying','operations':[{'dest':str(dest),'backup':str(backup),'temp':str(target/'temp'),'original':True,'started':True}]})
        u.rollback(job);u.rollback(job)
        self.assertEqual(dest.read_text(),'old')
    @unittest.skipIf(os.name == "nt", "POSIX filesystem symlink regression")
    def test_destination_parent_symlink_refused(self):
        target=self.root/'target';target.mkdir();outside=self.root/'outside';outside.mkdir()
        (target/'link').symlink_to(outside,target_is_directory=True)
        with self.assertRaises(ValueError):u.destination(target,'link/test')
    @unittest.skipIf(os.name == "nt", "POSIX filesystem symlink regression")
    def test_linux_existing_library_symlink_can_be_replaced_and_rolled_back(self):
        target,job,plan=self.plan();(target/'old').unlink();(target/'real').write_text('original');(target/'old').symlink_to('real')
        with self.assertRaises(OSError):u.commit(job,plan,fail_after=0)
        self.assertTrue((target/'old').is_symlink());self.assertEqual((target/'old').read_text(),'original')
    def test_http_prepare_both_independent_versions_and_protected_files(self):
        web=self.root/'web';web.mkdir();data=self.root/'Data Files';data.mkdir();client=self.root/'client';client.mkdir();job=self.root/'job';job.mkdir()
        content=self.zip({'Textures/a.dds':b'content'});content.rename(web/'update.zip')
        engine=self.zip({'openmw-launcher.exe':b'launcher','tes3mp.exe':b'engine','build.ini':b'bad','server/data/player/alice.json':b'bad','openmw.cfg':b'bad','settings.cfg':b'bad','userdata/save':b'bad','resources/file':b'resource'},'engine.zip');engine.rename(web/'engine.zip')
        class Quiet(http.server.SimpleHTTPRequestHandler):
            def log_message(self,*args): pass
        server=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(Quiet,directory=str(web)))
        thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
        try:
            base='http://127.0.0.1:'+str(server.server_port)
            with self.assertRaises(ValueError):
                u.download(base+'/update.zip', self.root/'bad-hash.zip', expected_hash='0'*64)
            mf=self.root/'build.ini';mf.write_text('[Build]\nversion=00001\nbuild=00001\nurl_check='+base+'/check.ini\nurl_update='+base+'/update.zip\nurl_win='+base+'/engine.zip\n[Server]\naddress=178.20.47.31\n')
            request={'manifest':str(mf),'data':str(data),'client':str(client),'parent_pid':0,'engine_key':'url_win'}
            # Missing check must not prevent launching and must not alter any revisions.
            self.assertEqual(u.prepare(request,job),0)
            # Equal revisions are the normal no-update path. It must return
            # immediately without a pending transaction, so the launcher can
            # continue and invoke tes3mp itself.
            (web/'check.ini').write_text('version=00001\nbuild=00001\n')
            equal_job = self.root/'equal-job'; equal_job.mkdir()
            self.assertEqual(u.check(request),0)
            self.assertEqual(u.prepare(request,equal_job),0)
            self.assertFalse((mf.parent/'.arena-update-pending.json').exists())
            (web/'check.ini').write_text('version=00002\nbuild=00003\n')
            self.assertEqual(u.check(request),10)
            self.assertEqual(u.prepare(request,job),10)
            plan=json.loads((job/'plan.json').read_text());self.assertEqual(plan['versions'],{'version':'00002','build':'00003'})
            self.assertEqual(plan['files'][-1]['relative'],'build.ini')
            u.commit(job,plan)
            self.assertEqual((data/'Textures/a.dds').read_bytes(),b'content')
            self.assertEqual((client/'tes3mp.exe').read_bytes(),b'engine')
            for protected in ('build.ini','server','openmw.cfg','settings.cfg','userdata'):
                self.assertFalse((client/protected).exists())
            self.assertIn('address=178.20.47.31',mf.read_text())
            self.assertIn('version=00002',mf.read_text());self.assertIn('build=00003',mf.read_text())
        finally:server.shutdown();server.server_close();thread.join()

    def test_supervisor_no_update_reopens_launcher_with_resume(self):
        """The Update button must never leave the GUI closed on a stale check."""
        manifest = self.root / 'build.ini'
        manifest.write_text('[Build]\nversion=00001\nbuild=00001\n')
        data = self.root / 'Data Files'; data.mkdir()
        job = self.root / 'job'; job.mkdir()
        started = []
        original_popen = u.subprocess.Popen
        u.subprocess.Popen = lambda args, **kwargs: started.append((args, kwargs))
        request = {
            'manifest': str(manifest), 'data': str(data), 'client': str(self.root),
            'launcher': 'launcher', 'launcher_args': ['--normal-arg'],
            'parent_pid': 0, 'engine_key': 'url_linux'
        }
        try:
            self.assertEqual(u.update(request, job), 0)
        finally:
            u.subprocess.Popen = original_popen
        self.assertEqual(len(started), 1)
        self.assertEqual(started[0][0], ['launcher', '--normal-arg', '--arena-update-resume'])
        self.assertEqual(started[0][1]['cwd'], str(self.root))

if __name__=='__main__':unittest.main()
