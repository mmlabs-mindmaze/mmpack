detours_sources = files(
    'detours.h',
    'detver.h',
    'creatwth.cpp',
    'detours.cpp',
    'disasm.cpp',
    'disolarm.cpp',
    'disolarm64.cpp',
    'disolia64.cpp',
    'disolx64.cpp',
    'disolx86.cpp',
    'image.cpp',
    'modules.cpp',
    'uimports.cpp',
)

detours = static_library('detours',
    detours_sources,
    include_directories : inc,
)
