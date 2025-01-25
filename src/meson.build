lib_sources = files('ccache.c')

ccache_lib = shared_library(
  'ccache_lib',       
  lib_sources,
  include_directories: inc
)

main_sources = files('main.c', 'ram_bench.c')

executable(
  'ccache_app',
  main_sources,
  link_with: ccache_lib,
  include_directories: inc
)

