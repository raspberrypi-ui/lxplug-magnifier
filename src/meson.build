gtk = dependency('gtk+-3.0')
gtkmm = dependency('gtkmm-3.0', version: '>=3.24')

lsources = files(
  'magnifier.c'
)

ldeps = [ gtk ]

lincdir = include_directories('/usr/include/lxpanel')

largs = [ '-DLXPLUG', '-DPACKAGE_DATA_DIR="' + lresource_dir + '"', '-DGETTEXT_PACKAGE="lxplug_' + meson.project_name() + '"' ]

shared_module(meson.project_name(), lsources,
        dependencies: ldeps,
        install: true,
        install_dir: get_option('libdir') / 'lxpanel/plugins',
        c_args : largs,
        include_directories : lincdir,
        name_prefix: ''
)
