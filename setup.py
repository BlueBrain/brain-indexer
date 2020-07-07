import os
import re
import sys
import subprocess
from distutils.version import LooseVersion
from setuptools import Command
from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext


# Main source of the version. Dont rename, used by Cmake
try:
    __version__ = subprocess.run(['git', 'describe', '--tags'],
                                 stdout=subprocess.PIPE).stdout.strip().decode()
    if '-' in __version__: __version__ = __version__[:-9]  # noqa
except Exception as e:
    raise RuntimeError("Could not get version from Git repo") from e


class CMakeExtension(Extension):
    def __init__(self, name, sourcedir='', cmake_opts=None):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)
        self.cmake_opts = cmake_opts or []


class CMakeBuild(build_ext):
    def run(self):
        cmake = self._find_cmake()
        for ext in self.extensions:
            self.build_extension(ext, cmake)

    def build_extension(self, ext, cmake):
        self.outdir = os.path.abspath(os.path.dirname(
            self.get_ext_fullpath(ext.name)))
        print("Building lib to:", self.outdir)
        cmake_args = [
            '-DEXTENSION_OUTPUT_DIRECTORY=' + self.outdir,
            '-DPYTHON_EXECUTABLE=' + sys.executable
        ] + ext.cmake_opts

        cfg = 'Debug' if self.debug else 'Release'
        cmake_args += ['-DCMAKE_BUILD_TYPE=' + cfg]
        build_args = ['--config', cfg, '--', '-j4']

        env = os.environ.copy()
        env['CXXFLAGS'] = "{} -static-libstdc++ -DVERSION_INFO='{}'".format(
            env.get('CXXFLAGS', ''),
            self.distribution.get_version())
        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)
        try:
            subprocess.Popen(
                "echo $CXX", shell=True, stdout=subprocess.PIPE)
            subprocess.check_call([cmake, ext.sourcedir] + cmake_args,
                                  cwd=self.build_temp, env=env)
            subprocess.check_call([cmake, '--build', '.'] + build_args,
                                  cwd=self.build_temp)
        except subprocess.CalledProcessError as exc:
            print("Status : FAIL", exc.returncode, exc.output)
            raise

    @staticmethod
    def _find_cmake():
        for candidate in ['cmake', 'cmake3']:
            try:
                out = subprocess.check_output([candidate, '--version'])
                cmake_version = LooseVersion(
                    re.search(r'version\s*([\d.]+)', out.decode()).group(1))
                if cmake_version >= '3.5.0':
                    return candidate
            except OSError:
                pass

        raise RuntimeError("Project requires CMake >=3.5.0")


class Docs(Command):
    description = "Generate & optionally upload documentation to docs server"
    user_options = [("upload", None, "Upload to BBP internal docs server")]
    finalize_options = lambda self: None

    def initialize_options(self):
        self.upload = False

    def run(self):
        self._create_metadata_file()
        self.reinitialize_command('build_ext', inplace=1)
        self.run_command('build_ext')
        self.run_command('build_sphinx')  # requires metadata file
        if self.upload:
            self._upload()

    def _create_metadata_file(self):
        import textwrap
        import time
        md = self.distribution.metadata
        with open("docs/metadata.md", "w") as mdf:
            mdf.write(textwrap.dedent(f"""\
                ---
                name: {md.name}
                version: {md.version}
                description: {md.description}
                homepage: {md.url}
                license: {md.license}
                maintainers: {md.author}
                repository: {md.project_urls.get("Source", '')}
                issuesurl: {md.project_urls.get("Tracker", '')}
                contributors: {md.maintainer}
                updated: {time.strftime("%d/%m/%Y")}
                ---
                """))

    def _upload(self):
        from docs_internal_upload import docs_internal_upload
        print("Uploading....")
        docs_internal_upload("docs/_build/html", metadata_path="docs/metadata.md")


def setup_package():
    # sphinx-bluebrain-theme depends on a specific sphinx version. Let it choose
    docs_require = ["sphinx-bluebrain-theme", "docs_internal_upload"]
    maybe_docs = docs_require if "docs" in sys.argv else []
    maybe_test_runner = ['pytest-runner'] if "test" in sys.argv else []

    setup(
        name='spatial-index',
        version=__version__,
        packages=["spatial_index"],
        ext_modules=[CMakeExtension(
            'spatial_index._spatial_index',
            cmake_opts=[
                '-DSI_UNIT_TESTS=OFF',
                '-DSI_VERSION=' + __version__
            ]
        )],
        cmdclass=dict(build_ext=CMakeBuild, docs=Docs),
        include_package_data=True,
        install_requires=['numpy>=1.13.1'],
        tests_require=["flake8", "pytest"],
        setup_requires=maybe_docs + maybe_test_runner,
        dependency_links=[
            "https://bbpteam.epfl.ch/repository/devpi/simple/docs_internal_upload"]
    )


if __name__ == "__main__":
    setup_package()
