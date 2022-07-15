detours_sources = files(
    'detours.h',
    'detver.h',
    'creatwth.cpp',
    'detours.cpp',
    'disasm.cpp',
    'disolx64.cpp',
    'disolx86.cpp',
    'image.cpp',
    'modules.cpp',
)

detours = static_library('detours',
    detours_sources,
    include_directories : inc,
)
