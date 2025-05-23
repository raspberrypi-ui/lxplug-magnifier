gtk = dependency('gtk+-3.0')
lxpanel = dependency('lxpanel-pi')

lsources = files(
  'magnifier.c'
)

ldeps = [ gtk, lxpanel ]

largs = [ '-DPACKAGE_DATA_DIR="' + lresource_dir + '"', '-DGETTEXT_PACKAGE="lpplug_' + meson.project_name() + '"' ]

shared_module(meson.project_name(), lsources,
        dependencies: ldeps,
        install: true,
        install_dir: get_option('libdir') / 'lxpanel-pi',
        c_args : largs,
        name_prefix: ''
)
