"""Exercise checkout safeguards against disposable real Git repositories."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    'bootstrap', Path(__file__).resolve().parents[1] / 'environment/bootstrap.py')
bootstrap = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bootstrap)


class CheckoutTests(unittest.TestCase):
    """An interrupted fetch can resume; existing source must never be overwritten."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.source = self.base / 'source'
        self.source.mkdir()
        self.git('init', '-q', cwd=self.source)
        self.git('config', 'user.name', 'Lab test', cwd=self.source)
        self.git('config', 'user.email', 'test@example.invalid', cwd=self.source)
        (self.source / 'input.txt').write_text('original')
        self.git('add', '.', cwd=self.source)
        self.git('commit', '-qm', 'fixture', cwd=self.source)
        self.commit = self.git('rev-parse', 'HEAD', cwd=self.source)
        self.item = {'url': self.source.as_uri(), 'commit': self.commit}
        self.target = self.base / 'checkout'

    def git(self, *args, cwd):
        """Use Git directly rather than replacing subprocess behavior in tests."""
        return subprocess.check_output(['git', *args], cwd=cwd, text=True).strip()

    def test_fresh_checkout_and_repeat(self):
        bootstrap.checkout(self.target, self.item)
        bootstrap.checkout(self.target, self.item)
        self.assertEqual((self.target / 'input.txt').read_text(), 'original')
        self.assertEqual(self.git('rev-parse', 'HEAD', cwd=self.target), self.commit)

    def test_interrupted_fetch_resumes(self):
        self.target.mkdir()
        self.git('init', '-q', cwd=self.target)
        self.git('remote', 'add', 'origin', self.item['url'], cwd=self.target)
        bootstrap.checkout(self.target, self.item)
        self.assertEqual((self.target / 'input.txt').read_text(), 'original')

    def test_local_changes_are_preserved_and_rejected(self):
        bootstrap.checkout(self.target, self.item)
        (self.target / 'input.txt').write_text('local changes')
        with self.assertRaises(subprocess.CalledProcessError):
            bootstrap.checkout(self.target, self.item)
        self.assertEqual((self.target / 'input.txt').read_text(), 'local changes')

    def test_wrong_commit_is_rejected(self):
        bootstrap.checkout(self.target, self.item)
        with self.assertRaisesRegex(RuntimeError, 'expected'):
            bootstrap.checkout(self.target, dict(self.item, commit='0' * 40))

    def test_wrong_remote_is_rejected(self):
        bootstrap.checkout(self.target, self.item)
        with self.assertRaisesRegex(RuntimeError, 'origin'):
            bootstrap.checkout(self.target, dict(self.item, url='file:///wrong'))

    def test_nonempty_directory_is_preserved(self):
        self.target.mkdir()
        (self.target / 'notes').write_text('keep me')
        with self.assertRaisesRegex(RuntimeError, 'nonempty'):
            bootstrap.checkout(self.target, self.item)
        self.assertEqual((self.target / 'notes').read_text(), 'keep me')


if __name__ == '__main__':
    unittest.main()
