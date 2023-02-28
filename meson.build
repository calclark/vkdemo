project(
  'vkdemo',
  'cpp',
  default_options: [
    'buildtype=custom',
    'debug=true',
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

cxx = meson.get_compiler('cpp')
extra_args = cxx.get_supported_arguments(
  '-march=native',
  '-Ofast',
  '-ggdb3',
  '-pipe',
)

subdir('shaders')
subdir('assets')
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
  cmake.subproject('glfw', options: glfw_opts).dependency('glfw'),
]

include_directories = [
  'include/glm',
  'include/stb',
  'include/tinyobjloader',
  'include/Vulkan-Headers/include',
]

out = executable(
  'demo',
  sources,
  dependencies: dependencies,
  cpp_args: extra_args,
  include_directories: include_directories,
)

