"""Build script."""

from setuptools import Extension
from setuptools.command.build_ext import build_ext


extensions = [
    Extension(
        name="pywinmonitor.pymonitor",
        sources=[
            "./external/hashmap.c/hashmap.c",
            "./pywinmonitor/pymonitor/kvmap.c",
            "./pywinmonitor/pymonitor/fsmonitor.c",
            "./pywinmonitor/pymonitor/pymonitor.c",
        ],
        include_dirs=["./external/hashmap.c", "./pywinmonitor/pymonitor"],
        language="c",
    )
]


class BuildFailed(Exception):
    pass


class ExtBuilder(build_ext):
    def run(self):
        try:
            build_ext.run(self)
        except Exception as ex:
            print(f"[run] Error: {ex}")

    def build_extension(self, ext):
        try:
            build_ext.build_extension(self, ext)
        except Exception as ex:
            print(f"[build] Error: {ex}")


def build(setup_kwargs):
    setup_kwargs.update(
        {"ext_modules": extensions, "cmdclass": {"build_ext": ExtBuilder}}
    )


# def build(setup_kwargs):
#     setup_kwargs.update(
#         {
#             "ext_modules": extensions,
#             "cmdclass": {"build_ext": ExtBuilder},
#             "options": {"build_ext": {"compiler": "mingw32"}},
#         }
#     )
