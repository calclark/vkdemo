project(
  'vkdemo',
  'cpp',
  default_options: [
    'buildtype=custom',
    'debug=true',
    'default_library=static',
    'optimization=3',
    'prefer_static=true',
    'warning_level=3',
    'b_lto=true',
    'b_ndebug=true',
    'b_staticpic=false',
    'b_pie=false',
    'cpp_std=c++20',
    'cpp_eh=none',
    'cpp_rtti=false',
  ]
)

if meson.get_compiler('cpp').get_id() == 'gcc'
  extra_args = ['-march=native', '-Ofast', '-ggdb3', '-pipe']
else
  extra_args = []
endif

sources = [
  'src/main.cpp',
]

cmake = import('cmake')
dependencies = [
  cmake.subproject('fmt').dependency('fmt')
]

out = executable(
  'demo',
  sources,
  dependencies: dependencies,
  cpp_args: extra_args,
)

run_target('run', command: out)
run_target('r', command: out)

