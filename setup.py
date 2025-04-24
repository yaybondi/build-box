#!/usr/bin/env python3

"""Bondi OS build sandbox management utility"""

import os

from setuptools import setup

here = os.path.abspath(os.path.dirname(__file__))

VERSION = os.environ.get("BUILD_BOX_VERSION", "0.0.0")

setup(
    name='build-box-utils',
    version=VERSION,
    url='https://github.com/yaybondi/build-box-utils',
    author='Tobias Koch',
    author_email='tobias.koch@gmail.com',
    license='MIT',
    packages=[
        'yaybondi.buildbox',
        'yaybondi.buildbox.misc',
    ],
    package_dir={'': 'lib'},
    data_files=[
        ('/usr/bin', ['bin/build-box']),
    ],
    platforms=['Linux'],

    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'Topic :: Software Development :: Build Tools',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: 3'
    ],

    keywords='Bondi OS cross-compile bootstrap',
    description='Bondi OS build sandbox management utility',
    long_description='Bondi OS build sandbox management utility',
)
