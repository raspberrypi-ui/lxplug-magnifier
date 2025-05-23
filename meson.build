project(
    'magnifier',
    'c',
    meson_version: '>=0.60.0',
    default_options: [
        'c_std=c11',
        'warning_level=2',
        'werror=false',
    ],
)

share_dir = join_paths(get_option('prefix'), 'share')
lresource_dir = join_paths(share_dir, 'lxpanel')
wresource_dir = join_paths(share_dir, 'wf-panel-pi')
metadata_dir = join_paths(wresource_dir, 'metadata')

add_project_arguments('-DPACKAGE_LOCALE_DIR="' + share_dir + '/locale"', language : [ 'c', 'cpp' ])
add_project_arguments('-D_GNU_SOURCE', language : [ 'c', 'cpp' ])
add_project_arguments('-DPLUGIN_NAME="' + meson.project_name() + '"', language : [ 'c', 'cpp' ])

subdir('src')
subdir('po')
