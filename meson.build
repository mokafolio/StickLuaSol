project('StickLuaSol', 'cpp', default_options : ['cpp_std=c++14'])

#remove this rather bogus warning
add_project_arguments('-Wno-missing-braces', language: 'cpp')

incDirs = include_directories('.')

install_headers('StickLuaSol/StickLuaSol.hpp', subdir: 'StickLuaSol')

stickLuaDep = declare_dependency(include_directories: incDirs)
