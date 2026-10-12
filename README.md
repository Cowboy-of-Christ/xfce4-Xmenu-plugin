# XFCE4 Apple Menu Plugin

A panel plugin for XFCE4 that mimics the macOS Apple menu, providing a familiar and intuitive system menu experience for XFCE desktop users.

![XFCE4 Apple Menu Plugin]

## Features

- 🍎 **Apple-style Menu**: Familiar dropdown menu with Command logo
- 💻 **System Information**: Quick access to system details
- 📦 **Package Management**: Integration with system package manager
- 📄 **Recent Items**: Track and access recently used files and applications
- ⚡ **Quick Actions**: Sleep, restart, shutdown, lock screen, and log out
- 🎨 **Themeable**: Supports GTK+ theming and custom icons
- 🌍 **Internationalization**: Multi-language support

## Menu Structure

- **About This Computer** - System information and details
- **System Preferences** - Quick access to XFCE Settings Manager
- **Software Update** - Launch your configured software updater
- **App Store** - Launch system package manager
- **Recent Items** - Recently accessed documents and applications
- **Force Quit** - Application management
- **System Actions** - Sleep, Restart, Shut Down, Lock Screen, Log Out

## Requirements

### Runtime Dependencies
- XFCE4 Panel (>= 4.16)
- GTK+3 (>= 3.24)
- libxfce4util (>= 4.16)
- libxfce4ui (>= 4.16)
- System package manager (for App Store functionality)

### Build Dependencies
- meson (>= 0.49.0)
- ninja-build
- gcc or clang
- pkg-config
- libxfce4panel-2.0-dev (>= 4.16)
- libxfce4ui-2-dev (>= 4.16)
- libxfce4util-dev (>= 4.16)
- libgtk-3-dev (>= 3.24)
- libglib2.0-dev (>= 2.66)
- libexo-2-dev (>= 4.16, optional)
- libdbus-glib-1-dev (>= 0.110, optional)

### Installing Build Dependencies on Debian 11

```bash
sudo apt update
sudo apt install meson ninja-build gcc pkg-config \
    libxfce4panel-2.0-dev libxfce4ui-2-dev \
    libxfce4util-dev libgtk-3-dev libglib2.0-dev \
    libexo-2-dev libdbus-glib-1-dev
```

## Installation

### From Source

```bash
# Clone the repository
git clone https://github.com/Axis0S/xfce4-applemenu-plugin.git
cd xfce4-applemenu-plugin

# Build and install using Meson
meson setup build --prefix=/usr
cd build
ninja
sudo ninja install

# Or use the provided install script
./install.sh

# Restart XFCE Panel
xfce4-panel -r
```

### Debian/Ubuntu

```bash
# Build and install Debian package
./build-deb.sh
sudo dpkg -i ../xfce4-applemenu-plugin_*.deb
```

## Automatic Builds (Devuan)

The **Build Devuan packages** GitHub Actions workflow builds installable `.deb` packages for **Devuan Daedalus (5)** and **Devuan Excalibur (6)** on **amd64**. Each build uses the corresponding official `devuan/devuan` container, packages the plugin with the existing Debian packaging, and checks package installation and linked runtime libraries. It does not require systemd to run.

Builds run automatically on pushes to `main` (including PR merges) and pull requests targeting `main`. PR builds test GitHub's temporary merge with the base branch.

Once the workflow is merged into `main`, you can also run it manually:

1. Open **Actions → Build Devuan packages → Run workflow**.
2. Select a branch, or enter an optional **pull_request** number to build that PR's latest source without merging it. A manual PR build uses the PR head, rather than the temporary merge.
3. Open the completed run and download the artifact for your Devuan release. Extract the ZIP to obtain the `.deb`, checksums and build information. Artifacts are retained for 30 days.
4. Install the main package with `sudo apt install ./xfce4-applemenu-plugin_<version>_amd64.deb`, then restart the panel with `xfce4-panel -r`.

Use the package matching your system's Devuan base release. For derivatives, confirm that the base release and Xfce/GTK libraries match; a successful CI build does not prove runtime compatibility with a customized desktop or XLibre setup. These builds do not test the live panel UI. If unsure, inspect `/etc/os-release` and `apt-cache policy xfce4-panel libc6`. ARM and 32-bit systems are not covered.

CI versions include the workflow run, attempt, target release and source commit so test packages are identifiable. The workflow creates downloadable build artifacts; it does not publish a GitHub Release.

## Configuration

Right-click on the plugin in the panel and select "Properties" to access:

- Icon selection (Command logo or custom)
- Commands for About This Computer, Software Update, App Store, and Force Quit
- Recent items settings
- Menu behavior options
- Keyboard shortcuts

### Menu Commands

In Properties, enter the executable and any arguments for each menu item:

- **About This Computer command**: for example, `xfce4-about` or `hardinfo`. Leave empty to use the built-in information dialog.
- **Software Update command**: enter your installed updater executable and any arguments. Initially empty; selecting Software Update without a command prompts you to configure it in Properties.
- **Force Quit command**: for example, `xfce4-taskmanager`. Defaults to `xkill`; an empty field also uses `xkill`.
- **App Store command**: your preferred package manager (see below).

The selected programs must be installed. Commands support arguments and quoted paths, using the same launcher as App Store. Shell operators such as pipes and redirection are not interpreted; use an explicit `sh -c '...'` command if needed. Settings are saved when the Properties dialog closes and restored when the panel restarts.

## Package Manager Integration

The "App Store" menu item can be configured to launch your preferred package manager through the plugin's Properties dialog. Common options include:
- pamac-manager (Arch-based distributions)
- gnome-software (GNOME)
- synaptic (Debian/Ubuntu)
- kde-discover (KDE)

## Theming

The plugin respects your GTK+ theme. For custom styling, add to `~/.config/gtk-3.0/gtk.css`:

```css
#xfce4-applemenu-plugin button {
    padding: 4px 8px;
    border-radius: 4px;
}

#xfce4-applemenu-plugin button:hover {
    background-color: @theme_selected_bg_color;
}
```

## Troubleshooting

### Plugin not appearing
1. Ensure the plugin is properly installed: `ls /usr/lib/xfce4/panel/plugins/`
2. Add it through Panel Preferences → Items → Add → Apple Menu

### Package manager not launching
1. Verify your package manager is installed
2. Configure the correct package manager in plugin Properties

### Icons not displaying
1. Install an icon theme with Command logo support
2. Or place custom Command logo in `~/.local/share/icons/`

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Follow XFCE coding standards
4. Submit a pull request

See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed guidelines.

## License

This project is licensed under the GPL-2.0+ License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- XFCE Development Team for the panel plugin framework
- Apple Inc. for the menu design inspiration
- Package manager developers
- Contributors and translators

## Links

- [Documentation](DOCUMENT.md)
- [XFCE Panel Plugin Guide](https://docs.xfce.org/xfce/xfce4-panel/theming)
- [Report Issues](https://github.com/Axis0S/xfce4-applemenu-plugin/issues)

## Author

Kamil 'Novik' Nowicki
- Email: novik@axisos.org
- Website: https://axisos.org
- GitHub: https://github.com/Axis0S
