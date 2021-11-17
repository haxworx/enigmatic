inc = include_directories('.')

src_cpu = []

src_cpu += src_client

src_cpu += files([
   'cpu.c',
])

executable('cpeew', src_cpu,
   include_directories     : [ inc, inc_client ],
   dependencies            : [ dep_client, dep_elm ],
   link_with               : lz4_lib,
   gui_app                 : false,
   install                 : true)


src_blind = []
src_blind += src_client
src_blind += files([
   'blindmin.c',
])

executable('blindmin', src_blind,
   include_directories     : [ inc, inc_client ],
   dependencies            : [ dep_client, dep_elm ],
   link_with               : lz4_lib,
   gui_app                 : false,
   install                 : true)
