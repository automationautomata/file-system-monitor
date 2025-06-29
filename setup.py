# -*- coding: utf-8 -*-
from setuptools import setup
import build

packages = ["pyfsmonitor"]

package_data = {"": ["*"]}

setup_kwargs = {
    "name": "pyfsmonitor",
    "version": "0.1.1",
    "description": "",
    "long_description": "None",
    "author": "None",
    "author_email": "None",
    "maintainer": "None",
    "maintainer_email": "None",
    "url": "None",
    "packages": packages,
    "package_data": package_data,
    "python_requires": ">=3.12,<4.0",
}

build.build(setup_kwargs)

setup(**setup_kwargs)
