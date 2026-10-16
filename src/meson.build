# Plugin sources
xmenu_sources = [
  'xmenu.c',
  'xmenu.h'
]

# Dependencies for the plugin
xmenu_deps = [
  glib_dep,
  gtk_dep,
  libxfce4panel_dep,
  libxfce4ui_dep,
  libxfce4util_dep,
]

if exo_dep.found()
  xmenu_deps += exo_dep
endif

if dbus_dep.found()
  xmenu_deps += dbus_dep
endif

# Build the plugin as a shared module
xmenu_lib = shared_module('xmenu',
  xmenu_sources,
  dependencies: xmenu_deps,
  include_directories: inc,
  install: true,
  install_dir: get_option('libdir') / 'xfce4' / 'panel' / 'plugins',
  name_prefix: 'lib',
  c_args: [
    '-DG_LOG_DOMAIN="xfce4-xmenu-plugin"',
  ]
)

# Desktop file
desktop_file = i18n.merge_file(
  input: 'xmenu.desktop.in',
  output: 'xmenu.desktop',
  type: 'desktop',
  po_dir: '../po',
  install: true,
  install_dir: get_option('datadir') / 'xfce4' / 'panel' / 'plugins'
)
