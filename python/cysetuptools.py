import subprocess
import os
import os.path as op

import setuptools

import sys
PY3 = sys.version_info[0] == 3
if PY3:
    import configparser
else:
    import ConfigParser as configparser


DEFAULTS_SECTION = 'cython-modules'
MODULE_SECTION_PREFIX = 'cython-module:'
CYTHON_EXT = '.pyx'
C_EXT = '.c'
CPP_EXT = '.cpp'


def setup(cythonize=True, **kwargs):
    """
    Drop-in replacement for :func:`setuptools.setup`, adding Cython niceties.

    Cython modules are described in setup.cfg, for example::

        [cython-module: foo.bar]
        sources = foo.pyx
                  bar.cpp
        include_dirs = eval(__import__('numpy').get_include())
                       /usr/include/foo
        language = c++
        pkg_config_packages = openscenegraph-osg

    The modules sections support the following entries:

    sources
        The list of Cython and C/C++ source files that are compiled to build
        the module.

    libraries
        A list of libraries to link with the module.

    include_dirs
        A list of directories to find include files. This entry supports
        python expressions with ``eval()``; in the example above this is used
        to retrieve the numpy include directory.

    library_dirs
        A list of directories to find libraries.

    extra_compile_args
        Extra arguments passed to the compiler.

    language
        "c" or "c++"; the default is "c"

    pkg_config_packages
        A list of ``pkg-config`` package names to link with the module.

    pkg_config_dirs
        A list of directories to add to the pkg-config search paths (extends
        the ``PKG_CONFIG_PATH`` environment variable).

    There are two approaches when distributing Cython modules: with or without
    the C files. Both approaches have their advantages and inconvenients:

        * not distributing the C files means they are generated on the fly when
          compiling the modules. Cython needs to be installed on the system,
          and it makes your package a bit more future proof, as Cython evolves
          to support newer Python versions. It also introduces some variance in
          the builds, as they now depend on the Cython version installed on the
          system;

        * when you distribute the C files with your package, the modules can be
          compiled directly with the host compiler, no Cython required. It also
          makes your tarball heavier, as Cython generates quite verbose code.
          It might also be good for performance-critical code, when you want to
          make sure the generated code is optimal, regardless of version of
          Cython installed on the host system.

    It is up to you to choose one option or the other. The *cythonize* argument
    controls the default mode of operation: set it to ``True`` if you don't
    distribute C files with your package (the default), and ``False`` if you
    do.

    Packages that distribute C files may use the ``CYTHONIZE`` environment
    variable to create or update the C files:

        CYTHONIZE=1 python setup.py build_ext --inplace

    You can also enable profiling for the Cython modules with the
    ``PROFILE_CYTHON`` environment variable::

        PROFILE_CYTHON=1 python setup.py build_ext --inplace

    """
    this_dir = op.dirname(__file__)
    setup_cfg_file = op.join(this_dir, 'setup.cfg')
    cythonize = _str_to_bool(os.environ.get('CYTHONIZE', cythonize))
    profile_cython = _str_to_bool(os.environ.get('PROFILE_CYTHON', False))
    if op.exists(setup_cfg_file):
        # Create Cython Extension objects
        with open(setup_cfg_file) as fp:
            parsed_setup_cfg = parse_setup_cfg(fp, cythonize=cythonize)
        cython_ext_modules = create_cython_ext_modules(
            parsed_setup_cfg['cython_modules'],
            profile_cython=profile_cython
        )
        ext_modules = kwargs.setdefault('ext_modules', [])
        ext_modules.extend(cython_ext_modules)

        # Use Cython's build ext if we're cythonizing
        if cythonize:
            try:
                from Cython.Distutils import build_ext
            except ImportError:
                # Dummy wrapper to avoid Cython import errors during setup
                def build_ext(*args, **kwargs):
                    pass
        else:
            from setuptools.command.build_ext import build_ext
        cmd_class = kwargs.setdefault('cmdclass', {})
        cmd_class.setdefault('build_ext', build_ext)

    setuptools.setup(**kwargs)


def create_cython_ext_modules(cython_modules, profile_cython=False):
    """
    Create :class:`~distutils.extension.Extension` objects from
    *cython_modules*.

    *cython_modules* must be a dict, as returned by :func:`parse_setup_cfg`.
    """
    if profile_cython:
        from Cython.Distutils import Extension
    else:
        from distutils.extension import Extension

    ret = []
    for name, mod_data in cython_modules.items():
        kwargs = {'name': name}
        kwargs.update(mod_data)
        if profile_cython:
            kwargs['cython_directives'] = {'profile': True}
        ext = Extension(**kwargs)
        ret.append(ext)
    return ret


def parse_setup_cfg(fp, cythonize=False, pkg_config=None, base_dir=''):
    """
    Parse the cython specific bits in a setup.cfg file.

    *fp* must be a file-like object opened for reading.

    *pkg_config* may be a callable taking a list of library names and returning
    a ``pkg-config`` like string (e.g. ``-I/foo -L/bar -lbaz``). The default is
    to use an internal function that actually runs ``pkg-config`` (normally
    used for testing).

    *base_dir* can be used to make relative paths absolute.
    """
    if pkg_config is None:
        pkg_config = _run_pkg_config
    config = configparser.SafeConfigParser()
    config.readfp(fp)
    ret = {}
    ret['cython_modules'] = _expand_cython_modules(config, cythonize,
                                                   pkg_config, base_dir)
    return ret


def _expand_cython_modules(config, cythonize, pkg_config, base_dir):
    ret = {}

    # Search for defaults
    for section in config.sections():
        if section == DEFAULTS_SECTION:
            default_opts = dict(config.items(section))

    # Expand modules
    for section in config.sections():
        if section.startswith(MODULE_SECTION_PREFIX):
            module_name = section[len(MODULE_SECTION_PREFIX):].strip()
            module_dict = _expand_one_cython_module(config, section, cythonize,
                                                    pkg_config, base_dir,
                                                    default_opts)
            ret[module_name] = module_dict

    return ret


def _expand_one_cython_module(config, section, cythonize, pkg_config,
                              base_dir, default_opts):
    module = {}
    module['language'] = _get_config_opt(config, section, 'language',
                                         default_opts, None)
    module['extra_compile_args'] = \
        _get_config_list(config, section, 'extra_compile_args', default_opts)
    module['sources'] = _expand_sources(config, section, module['language'],
                                        cythonize, default_opts)
    pc_include_dirs, pc_library_dirs, pc_libraries = \
        _expand_pkg_config_pkgs(config, section, pkg_config, default_opts)
    include_dirs = _get_config_list(config, section, 'include_dirs',
                                    default_opts)
    include_dirs = _eval_strings(include_dirs)
    include_dirs = _make_paths_absolute(include_dirs, base_dir)
    library_dirs = _get_config_list(config, section, 'library_dirs',
                                    default_opts)
    library_dirs = _make_paths_absolute(library_dirs, base_dir)
    libraries = _get_config_list(config, section, 'libraries', default_opts)
    module['include_dirs'] = include_dirs + pc_include_dirs
    module['library_dirs'] = library_dirs + pc_library_dirs
    module['libraries'] = libraries + pc_libraries
    return module


def _make_paths_absolute(paths, base_dir):
    return [op.join(base_dir, p) if not p.startswith('/') else p
            for p in paths]


def _eval_strings(values):
    ret = []
    for value in values:
        if value.startswith('eval(') and value.endswith(')'):
            ret.append(eval(value[5:-1]))
        else:
            ret.append(value)
    return ret


def _expand_pkg_config_pkgs(config, section, pkg_config, default_opts):
    pkg_names = _get_config_list(config, section, 'pkg_config_packages',
                                 default_opts)
    if not pkg_names:
        return [], [], []

    # update PKG_CONFIG_PATH
    original_pkg_config_path = os.environ.get('PKG_CONFIG_PATH', '')
    pkg_config_path = original_pkg_config_path.split(":")
    pkg_config_path.extend(_get_config_list(config, section,
                                            'pkg_config_dirs', default_opts))
    os.environ['PKG_CONFIG_PATH'] = ":".join(pkg_config_path)

    pkg_config_output = pkg_config(pkg_names)
    flags = pkg_config_output.split()
    include_dirs = [f[2:] for f in flags if f.startswith('-I')]
    library_dirs = [f[2:] for f in flags if f.startswith('-L')]
    libraries = [f[2:] for f in flags if f.startswith('-l')]

    # restore original PKG_CONFIG_PATH
    os.environ['PKG_CONFIG_PATH'] = original_pkg_config_path

    return include_dirs, library_dirs, libraries


def _run_pkg_config(pkg_names):
    return subprocess.check_output(['pkg-config', '--libs',
                                    '--cflags'] + pkg_names)


def _expand_sources(config, section, language, cythonize, default_opts):
    if cythonize:
        ext = CYTHON_EXT
    elif language == 'c':
        ext = C_EXT
    else:
        ext = CPP_EXT
    sources = _get_config_list(config, section, 'sources', default_opts)
    return [_replace_cython_ext(s, ext) for s in sources]


def _replace_cython_ext(filename, target_ext):
    root, ext = op.splitext(filename)
    if ext == CYTHON_EXT:
        return root + target_ext
    return filename


def _get_config_opt(config, section, option, default_opts, default):
    try:
        return config.get(section, option)
    except configparser.NoOptionError:
        try:
            return default_opts[option]
        except KeyError:
            return default


def _get_config_list(config, section, option, default_opts):
    value = _get_config_opt(config, section, option, default_opts, '')
    return value.split()


def _str_to_bool(value):
    if isinstance(value, bool):
        return value
    value = value.lower()
    if value in ('1', 'on', 'true', 'yes'):
        return True
    elif value in ('0', 'off', 'false', 'no'):
        return False
    raise ValueError('invalid boolean string %r' % value)
