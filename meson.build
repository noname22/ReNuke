project('renuke', 'c',
  version : '1.0.12',
  license : 'LGPL-2.1-or-later',
  default_options : [
    'c_std=c99',
    'warning_level=2'
  ],
  meson_version : '>= 0.50.0'
)

# Project metadata
pkg = import('pkgconfig')

# Compiler flags
cc = meson.get_compiler('c')
add_project_arguments(
  cc.get_supported_arguments([
    '-Wno-unused-parameter',
    '-Wno-sign-compare'
  ]),
  language : 'c'
)

# Include directories
inc = include_directories('include')

# Process subdirectories
subdir('include')
subdir('src')
subdir('examples')

# Summary
summary('Build type', get_option('buildtype'))
summary('Install prefix', get_option('prefix'))
summary('Build shared library', get_option('default_library') == 'shared' or get_option('default_library') == 'both')
summary('Build static library', get_option('default_library') == 'static' or get_option('default_library') == 'both')