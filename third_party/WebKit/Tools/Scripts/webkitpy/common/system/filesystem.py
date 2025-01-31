# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Wrapper object for functions that access the filesystem.

A FileSystem object can be used to represent dependency on the
filesystem, and can be replaced with a MockFileSystem in tests.
"""

import codecs
import errno
import exceptions
import glob
import hashlib
import logging
import os
import shutil
import stat
import sys
import tempfile
import time

_log = logging.getLogger(__name__)


class FileSystem(object):
    """FileSystem interface for webkitpy.

    Unless otherwise noted, all paths are allowed to be either absolute
    or relative.
    """
    sep = os.sep
    pardir = os.pardir

    def abspath(self, path):
        return os.path.abspath(path)

    def realpath(self, path):
        return os.path.realpath(path)

    def path_to_module(self, module_name):
        """Returns the absolute path of a module."""
        # FIXME: This is the only use of sys in this file. It's possible that
        # this function should move elsewhere.
        # __file__ is not always an absolute path in Python <3.4
        # (https://bugs.python.org/issue18416).
        return self.abspath(sys.modules[module_name].__file__)

    def expanduser(self, path):
        return os.path.expanduser(path)

    def basename(self, path):
        return os.path.basename(path)

    def chdir(self, path):
        return os.chdir(path)

    def copyfile(self, source, destination):
        shutil.copyfile(source, destination)

    def dirname(self, path):
        return os.path.dirname(path)

    def exists(self, path):
        return os.path.exists(path)

    def files_under(self, path, dirs_to_skip=None, file_filter=None):
        """Walks the filesystem tree under the given path in top-down order.

        Args:
            dirs_to_skip: A list of directories to skip over during the
                traversal (e.g., .svn, resources, etc.).
            file_filter: If not None, the filter will be invoked with the
                filesystem object and the dirname and basename of each file
                found. The file is included in the result if the callback
                returns True.

        Returns:
            A list of all files under the given path in top-down order.
        """
        dirs_to_skip = dirs_to_skip or []

        def filter_all(fs, dirpath, basename):
            return True

        file_filter = file_filter or filter_all
        files = []
        if self.isfile(path):
            if file_filter(self, self.dirname(path), self.basename(path)):
                files.append(path)
            return files

        if self.basename(path) in dirs_to_skip:
            return []

        for (dirpath, dirnames, filenames) in os.walk(path):
            for d in dirs_to_skip:
                if d in dirnames:
                    dirnames.remove(d)

            for filename in filenames:
                if file_filter(self, dirpath, filename):
                    files.append(self.join(dirpath, filename))
        return files

    def getcwd(self):
        return os.getcwd()

    def glob(self, path):
        return glob.glob(path)

    def isabs(self, path):
        return os.path.isabs(path)

    def isfile(self, path):
        return os.path.isfile(path)

    def isdir(self, path):
        return os.path.isdir(path)

    def join(self, *comps):
        return os.path.join(*comps)

    def listdir(self, path):
        return os.listdir(path)

    def walk(self, top, topdown=True, onerror=None, followlinks=False):
        return os.walk(top, topdown=topdown, onerror=onerror, followlinks=followlinks)

    def mkdtemp(self, **kwargs):
        """Creates and returns a uniquely-named directory.

        This is like tempfile.mkdtemp, but if used in a with statement
        the directory will self-delete at the end of the block (if the
        directory is empty; non-empty directories raise errors). The
        directory can be safely deleted inside the block as well, if so
        desired.

        Note that the object returned is not a string and does not support all
        of the string methods. If you need a string, coerce the object to a
        string and go from there.
        """
        class TemporaryDirectory(object):

            def __init__(self, **kwargs):
                self._kwargs = kwargs
                self._directory_path = tempfile.mkdtemp(**self._kwargs)

            def __str__(self):
                return self._directory_path

            def __enter__(self):
                return self._directory_path

            def __exit__(self, type, value, traceback):
                # Only self-delete if necessary.

                # FIXME: Should we delete non-empty directories?
                if os.path.exists(self._directory_path):
                    os.rmdir(self._directory_path)

        return TemporaryDirectory(**kwargs)

    def maybe_make_directory(self, *path):
        """Creates the specified directory if it doesn't already exist."""
        try:
            os.makedirs(self.join(*path))
        except OSError as error:
            if error.errno != errno.EEXIST:
                raise

    def move(self, source, destination):
        shutil.move(source, destination)

    def mtime(self, path):
        return os.stat(path).st_mtime

    def normpath(self, path):
        return os.path.normpath(path)

    def open_binary_tempfile(self, suffix=''):
        """Creates, opens, and returns a binary temp file.

        Returns a tuple of the file and the name.
        """
        temp_fd, temp_name = tempfile.mkstemp(suffix)
        f = os.fdopen(temp_fd, 'wb')
        return f, temp_name

    def open_binary_file_for_reading(self, path):
        return codecs.open(path, 'rb')

    def open_binary_file_for_writing(self, path):
        return file(path, 'wb')

    def read_binary_file(self, path):
        """Returns the contents of the file as a byte string."""
        with file(path, 'rb') as f:
            return f.read()

    def write_binary_file(self, path, contents):
        with file(path, 'wb') as f:
            f.write(contents)

    def open_text_tempfile(self, suffix=''):
        """Creates, opens, and returns a text temp file.

        Returns a tuple of the file and the name.
        """
        _, temp_name = tempfile.mkstemp(suffix)
        f = codecs.open(temp_name, 'w', 'utf8')
        return f, temp_name

    def open_text_file_for_reading(self, path):
        # Note: There appears to be an issue with the returned file objects not
        # being seekable. See:
        # http://stackoverflow.com/questions/1510188/can-seek-and-tell-work-with-utf-8-encoded-documents-in-python
        return codecs.open(path, 'r', 'utf8')

    def open_text_file_for_writing(self, path):
        return codecs.open(path, 'w', 'utf8')

    def read_text_file(self, path):
        """Returns the contents of the file as a Unicode string.

        The file is read assuming it is a UTF-8 encoded file with no BOM.
        """
        with codecs.open(path, 'r', 'utf8') as f:
            return f.read()

    def write_text_file(self, path, contents):
        """Writes the contents to the file at the given location.

        The file is written encoded as UTF-8 with no BOM.
        """
        with codecs.open(path, 'w', 'utf8') as f:
            f.write(contents)

    def sha1(self, path):
        contents = self.read_binary_file(path)
        return hashlib.sha1(contents).hexdigest()

    def relpath(self, path, start='.'):
        return os.path.relpath(path, start)

    class _WindowsError(exceptions.OSError):
        """Fake exception for Linux and Mac."""

    def remove(self, path, osremove=os.remove, retry=True):
        """Removes a file.

        On Windows, if a process was recently killed and it held on to a file,
        the OS will hold on to the file for a short while. This makes attempts
        to delete the file fail. To work around that, this method will retry
        for a few seconds until Windows is done with the file.
        """
        try:
            exceptions.WindowsError
        except AttributeError:
            exceptions.WindowsError = FileSystem._WindowsError

        retry_timeout_sec = 3.0
        sleep_interval = 0.1
        while True:
            try:
                osremove(path)
                return True
            except exceptions.WindowsError:
                time.sleep(sleep_interval)
                retry_timeout_sec -= sleep_interval
                if retry_timeout_sec < 0 and not retry:
                    raise

    def rmtree(self, path, ignore_errors=True, onerror=None):
        """Deletes the directory rooted at path, whether empty or not."""
        shutil.rmtree(path, ignore_errors=ignore_errors, onerror=onerror)

    def remove_contents(self, dirname):
        """Attempts to remove the contents of a directory tree.

        Args:
            dirname (string): Directory to remove the contents of.

        Returns:
            bool: True if the directory is now empty.
        """
        return _remove_contents(self, dirname)

    def copytree(self, source, destination):
        shutil.copytree(source, destination)

    def split(self, path):
        """Return (dirname, basename + '.' + ext)"""
        return os.path.split(path)

    def splitext(self, path):
        """Return (dirname + os.sep + basename, '.' + ext)"""
        return os.path.splitext(path)

    def make_executable(self, file_path):
        os.chmod(file_path, stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR | stat.S_IRGRP | stat.S_IXGRP)

    def symlink(self, source, link_name):
        """Create a symbolic link. Unix only."""
        os.symlink(source, link_name)


# _remove_contents is implemented in terms of other FileSystem functions. To
# allow it to be reused on the MockFileSystem object, we define it here and
# then call it in both FileSystem and MockFileSystem classes.
def _remove_contents(fs, dirname, sleep=time.sleep):
    # We try multiple times, because on Windows a process which is
    # currently closing could still have a file open in the directory.
    _log.info('Removing contents of %s', dirname)
    errors = []

    def onerror(func, path, exc_info):
        errors.append(path)
        _log.exception('Failed at %s %s: %r', func, path, exc_info)

    attempts = 0
    while attempts < 5:
        del errors[:]

        for name in fs.listdir(dirname):
            fullname = fs.join(dirname, name)

            isdir = True
            try:
                isdir = fs.isdir(fullname)
            except os.error:
                onerror(fs.isdir, fullname, sys.exc_info())
                continue

            if isdir:
                try:
                    _log.debug('Removing directory %s', fullname)
                    fs.rmtree(fullname, ignore_errors=False, onerror=onerror)
                except os.error:
                    onerror(fs.rmtree, fullname, sys.exc_info())
                    continue
            else:
                try:
                    _log.debug('Removing file %s', fullname)
                    fs.remove(fullname, retry=False)
                except os.error:
                    onerror(fs.remove, fullname, sys.exc_info())
                    continue

        if not errors:
            break

        _log.warning('Contents removal failed, retrying in 1 second.')
        attempts += 1
        sleep(1)

    # Check the path is gone.
    if not fs.listdir(dirname):
        return True

    _log.warning('Unable to remove %s', dirname)
    for dirpath, dirnames, filenames in fs.walk(dirname, onerror=onerror, topdown=False):
        for fname in filenames:
            _log.warning('File %s still in output dir.', fs.join(dirpath, fname))
        for dname in dirnames:
            _log.warning('Dir %s still in output dir.', fs.join(dirpath, dname))

    return False
