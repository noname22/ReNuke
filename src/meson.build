# Source files
sources = files('renuke.c')

# Build library (shared and/or static based on configuration)
renuke_lib = library('renuke',
  sources,
  include_directories : inc,
  install : true,
  version : meson.project_version(),
  soversion : '1'
)

# Create dependency object for use in other projects
renuke_dep = declare_dependency(
  link_with : renuke_lib,
  include_directories : inc
)

# Generate pkg-config file
pkg.generate(
  renuke_lib,
  name : 'renuke',
  description : 'ReNuke - High accuracy Yamaha YM3438 (OPN2) emulator',
  url : 'https://github.com/nukeykt/Nuked-OPN2',
  filebase : 'renuke',
  subdirs : 'renuke'
)