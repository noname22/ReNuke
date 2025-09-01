sdl3_dep = dependency('sdl3', required: false, method: 'pkg-config')
if not sdl3_dep.found()
  sdl3_dep = dependency('SDL3', required: false, method: 'pkg-config')
endif

if not sdl3_dep.found()
  warning('SDL3 not found, skipping vgm-player')
  subdir_done()
endif

vgm_player_exe = executable('vgm-player',
  'main.c',
  'emu76489.c',
  include_directories : inc,
  dependencies : sdl3_dep,
  link_with : renuke_lib,
  install : false
)