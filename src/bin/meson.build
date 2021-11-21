inc = include_directories('monitor')

src = []

subdir('system')
subdir('monitor')

src += src_system
src += src_monitor
src += src_process

src_log = files([
   'enigmatic_log.c',
   'enigmatic_log.h',
])

src += src_log

src_generic = files([
   'enigmatic_util.c',
])

src += src_generic

src += files([
   'Enigmatic.h',
   'enigmatic_main.c',
   'uid.c',
])

executable('enigmatic', src,
   include_directories  : [ config_dir, inc, inc_lz4 ],
   dependencies         : [ dep_eina, dep_ecore, dep_ecore_file, deps_os ],
   link_with            : lz4_lib,
   gui_app              : false,
   install              : true)

src_start = files([
   'enigmatic_start.c',
])

src_start += src_generic
src_start += src_system

executable('enigmatic_start', src_start,
   include_directories  : [ config_dir ],
   dependencies         : [ dep_eina, dep_ecore, dep_ecore_file ],
   gui_app              : false,
   install              : true)

if host_os == 'freebsd' or host_os == 'dragonfly'
   meson.add_install_script('perms.sh')
endif

subdir('client')
subdir('examples')
subdir('tests')
