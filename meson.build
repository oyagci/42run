project('42run', 'cpp',
	version: '0.1',
	default_options: [
	  'warning_level=3',
	  'cpp_std=gnu++17',
	  'cpp_args=-Wno-class-memaccess',
	  'c_args=-Wno-pedantic -Wno-unused-parameter -Wno-sign-compare -Wno-format-truncation',
	  'werror=false',
	  'optimization=2'])

conf_data = configuration_data()
conf_data.set('version', 'v0.0.2')
configure_inc = '.'

configure_file(input: 'config.h.in',
			   output: 'config.h',
			   configuration: conf_data)

incdirs = []
incdirs += include_directories(configure_inc)
incdirs += include_directories('src')
incdirs += include_directories('src/engine')
incdirs += include_directories('subprojects')
incdirs += include_directories('subprojects/tinygltf')

srcs = [
  'src/main.cpp',
  'src/stb_image.cpp',
  'src/engine/Texture.cpp',
  'src/engine/TextureManager.cpp',
  'src/engine/Cubemap.cpp',
  'src/engine/Engine.cpp',
  'src/engine/utils/Settings.cpp',
  'src/engine/ecs/ECSEngine.cpp',
  'src/engine/ecs/Entity.cpp',
  'src/engine/ui/TextRenderer.cpp',
  'src/engine/ui/Anchor.cpp',
  'src/engine/ui/Button.cpp',
  'src/engine/ui/UI.cpp',
  'src/engine/ui/SceneComponent.cpp',
  'src/engine/ui/UIScene.cpp',
  'src/engine/Framebuffer.cpp',
  'src/engine/Mesh.cpp',
  'src/engine/Batch.cpp',
]

tinygltf_src = [
  'src/engine/tinygltf.cpp'
]

srcs += tinygltf_src

cmake = import('cmake')
lazyGL = cmake.subproject('LazyGL')

deps = [
  lazyGL.dependency('LazyGL'),
  dependency('glew', required : true),
  dependency('glfw3', required : true),
  dependency('threads', required : true),
  dependency('GL', required : true),
  dependency('fmt', required: true),
  meson.get_compiler('cpp').find_library('dl'),
  meson.get_compiler('cpp').find_library('m'),
]

incdirs += [ include_directories('.') ]

if build_machine.system() == 'linux'
  deps_linux = [
	dependency('x11', required : true),
	dependency('xi', required : true),
	dependency('xrandr', required : true),
	meson.get_compiler('cpp').find_library('freetype'),
  ]
  deps += deps_linux
  incdirs += include_directories('/usr/include/freetype2/')
endif
if build_machine.system() == 'darwin'
  deps_macos = [
	meson.get_compiler('cpp')
	  .find_library('freetype', dirs: '~/.brew/lib')
  ]
  deps += deps_macos
  incdirs += include_directories('/Users/oyagci/.brew/include/freetype2') 
endif

executable('42run',
  srcs,
  include_directories : incdirs,
  dependencies: deps
)
