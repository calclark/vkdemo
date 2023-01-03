shaders = files(
  'shader.vert',
  'shader.frag',
)

glslc = find_program('glslc')
foreach shader : shaders
  custom_target(
    command: [glslc, '@INPUT@', '-o', '@OUTPUT@'],
    input: shader,
    output: '@PLAINNAME@.spv',
    build_by_default: true
  )
endforeach

