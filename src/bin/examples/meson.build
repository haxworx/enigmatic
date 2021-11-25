inc = include_directories('.')

src_cpu = []

src_cpu += src_client

src_cpu += files([
   'colors.c',
   'colors.h',
   'cpeew.c',
])

executable('cpeew', src_cpu,
   include_directories     : [ inc, inc_client ],
   dependencies            : [ dep_client, dep_elm ],
   link_with               : lz4_lib,
   gui_app                 : true,
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

src_memories = []
src_memories += src_client
src_memories += files([
   'memories.c',
])


executable('memories', src_memories,
   include_directories     : [ inc, inc_client ],
   dependencies            : [ dep_client, dep_elm ],
   link_with               : lz4_lib,
   gui_app                 : true,
   install                 : true)


src_test = src_client
src_test += files([
   'enigmatic_client.c',
])

executable('enigmatic_client', src_test,
   include_directories     : [ inc_client ],
   dependencies            : [ dep_client, dep_elm ],
   link_with               : lz4_lib,
   gui_app                 : false,
   install                 : true)

