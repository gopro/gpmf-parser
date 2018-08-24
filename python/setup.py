#!/usr/bin/env python
from setuptools import find_packages
from cysetuptools import setup
from gpmf import __version__


setup(
    name='gpmf',
    version=__version__,
    description='GoPro GPMF parser',
    keywords='GoPro metadata parser GPMF',
    author='Luper Rouch',
    author_email='luper.rouch@gmail.com',
    url='https://github.com/gopro/gpmf-parser',
    license='MIT',
    packages=find_packages(),
    include_package_data=True,
    zip_safe=False,
)
