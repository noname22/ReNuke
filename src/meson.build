# Source files
sources = files('ym3438.c')

# Build library (shared and/or static based on configuration)
nuked_opn2_lib = library('nuked-opn2',
  sources,
  include_directories : inc,
  install : true,
  version : meson.project_version(),
  soversion : '1'
)

# Create dependency object for use in other projects
nuked_opn2_dep = declare_dependency(
  link_with : nuked_opn2_lib,
  include_directories : inc
)

# Generate pkg-config file
pkg.generate(
  nuked_opn2_lib,
  name : 'nuked-opn2',
  description : 'High accuracy Yamaha YM3438 (OPN2) emulator',
  url : 'https://github.com/nukeykt/Nuked-OPN2',
  filebase : 'nuked-opn2',
  subdirs : 'nuked-opn2'
)