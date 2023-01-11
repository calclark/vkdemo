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
    'b_lto=false',
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

subdir('shaders')
subdir('textures')
sources = [
  'src/main.cpp',
]

cmake = import('cmake')
glfw_opts = cmake.subproject_options()
glfw_opts.add_cmake_defines({
  'BUILD_SHARED_LIBS': false,
  'GLFW_BUILD_EXAMPLES': false,
  'GLFW_BUILD_TESTS': false,
  'GLFW_BUILD_DOCS': false,
  'GLFW_VULKAN_STATIC': false,
})

dependencies = [
  cmake.subproject('fmt').dependency('fmt'),
  cmake.subproject('Vulkan-Headers').dependency('Vulkan-Headers'),
  cmake.subproject('glfw', options: glfw_opts).dependency('glfw'),
  cmake.subproject('glm').dependency('glm'),
]

include_directories = [
  'subprojects/stb',
]

out = executable(
  'demo',
  sources,
  dependencies: dependencies,
  cpp_args: extra_args,
  include_directories: include_directories,
)

