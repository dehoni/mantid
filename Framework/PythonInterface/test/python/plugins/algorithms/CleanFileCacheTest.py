#pylint: disable=invalid-name,too-many-public-methods,too-many-arguments
import unittest
from mantid.kernel import *
from mantid.api import *
from mantid.simpleapi import CreateCacheFilename

from testhelpers import run_algorithm

import os, sys, mantid, hashlib, tempfile, glob, shutil

class CleanFileCache(unittest.TestCase):
    
    def test1(self):
        """CleanFileCache: one prop
        """
        # create a temporary directory with fake cache files
        # and other files
        cache_root = tempfile.mkdtemp()
        _hash = lambda s: hashlib.sha1(s).hexdigest()
        cache1, sig = CreateCacheFilename(
            CacheDir = cache_root,
            OtherProperties = ["A=1", "B=2"]
        )
        cache2, sig = CreateCacheFilename(
            CacheDir = cache_root,
            OtherProperties = ["C=3"],
        )
        touch(cache1)
        touch(cache2)
        non_cache = [os.path.join(cache_root, f) for f in ["normal1.txt", "normal2.dat"]]
        for p in non_cache: touch(p)
        # print glob.glob(os.path.join(cache_root, '*'))
        # Execute
        code = "CleanFileCache(CacheDir = %r, AgeInDays = 0)" % cache_root
        code = "from mantid.simpleapi import CleanFileCache; %s" % code
        cmd = '%s -c "%s"' % (sys.executable, code)
        if os.system(cmd):
            raise RuntimeError("Failed to excute %s" % cmd)
        # executed?
        # self.assertTrue(alg_test.isExecuted())
        # Verify ....
        files_remained = glob.glob(os.path.join(cache_root, '*'))
        self.assertEqual(set(files_remained), set(non_cache))
        # remove the temporary directory
        shutil.rmtree(cache_root)
        return


def touch(f):
    with open(f, 'wt') as stream:
        stream.write('\n')
    return


if __name__ == '__main__':
    unittest.main()
