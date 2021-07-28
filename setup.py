#!/usr/bin/env python3

"""Bolt Linux build sandbox management utility"""

import os

from setuptools import setup

here = os.path.abspath(os.path.dirname(__file__))

VERSION = os.environ.get("BUILD_BOX_VERSION", "0.0.0")

setup(
    name='build-box-utils',
    version=VERSION,
    url='https://github.com/boltlinux/build-box-utils',
    author='Tobias Koch',
    author_email='tobias.koch@gmail.com',
    license='MIT',
    packages=[
        'boltlinux.buildbox',
        'boltlinux.buildbox.misc',
    ],
    package_dir={'': 'lib'},
    data_files=[
        ('/usr/bin', ['bin/build-box', 'bin/build-box-do']),
    ],
    platforms=['Linux'],

    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'Topic :: Software Development :: Build Tools',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: 3'
    ],

    keywords='Bolt Linux cross-compile bootstrap',
    description='Bolt Linux build sandbox management utility',
    long_description='Bolt Linux build sandbox management utility',
)
