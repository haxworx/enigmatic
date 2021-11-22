inc_client = [ include_directories('.', '../'), config_dir, inc_lz4 ]
dep_client = [ dep_eina, dep_ecore, dep_ecore_file, dep_efreet, dep_eio ]

src_client = []

src_client += src_log
src_client += src_stubby
src_client += src_generic
src_client += src_system

src_client += files([
   'enigmatic_client.c',
])

