project(
  'glew',
  'c',
  default_options: ['default_library=static'],
)

glew_inc = include_directories('include')

glew_c_args = ['-DGLEW_NO_GLU']
if get_option('openq4_sdl3_loader')
  glew_c_args += ['-DOPENQ4_GLEW_SDL3_LOADER']
endif

glew_lib = static_library(
  'glew',
  files('src/glew.c'),
  include_directories: glew_inc,
  c_args: glew_c_args,
)

glew_dep = declare_dependency(
  include_directories: glew_inc,
  link_with: glew_lib,
)
