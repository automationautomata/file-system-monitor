import sys

if "win" not in sys.platform.lower():
    raise Exception()

from pywinmonitor.pymonitor import *
