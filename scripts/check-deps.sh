#!/usr/bin/env bash
# Sprawdza zależności uart_app przez pkg-config i wypisuje gotowe polecenie
# instalacji dla wykrytej dystrybucji. Niczego nie instaluje.
#
# Użycie:
#   scripts/check-deps.sh [moduły pkg-config...]
#
# Zmienne:
#   PKG_CONFIG  - nazwa binarki (domyślnie pkg-config, akceptuje pkgconf)
#
# Moduł "opencv" jest symboliczny: skrypt akceptuje opencv5 (Arch) albo
# opencv4 (Debian/Fedora) i zgłasza brak dopiero, gdy nie ma żadnego z nich.

set -u

PKG_CONFIG="${PKG_CONFIG:-pkg-config}"

if [ "$#" -gt 0 ]; then
    modules=("$@")
else
    modules=(opencv sdl2 libavformat libavcodec libavutil libswresample)
fi

# check linux distro 
distro=""
distro_like=""
if [ -r /etc/os-release ]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    distro="${ID:-}"
    distro_like="${ID_LIKE:-}"
fi

family="unknown"
case " $distro $distro_like " in
    *" arch "*|*" manjaro "*|*" endeavouros "*|*" artix "*) family="arch" ;;
    *" debian "*|*" ubuntu "*|*" linuxmint "*|*" pop "*|*" raspbian "*) family="debian" ;;
    *" fedora "*|*" rhel "*|*" centos "*|*" rocky "*|*" alma "*) family="fedora" ;;
    *" opensuse "*|*" suse "*|*" sles "*) family="suse" ;;
    *" alpine "*) family="alpine" ;;
esac

# module names 
pkg_for_module() {
    local module="$1"
    case "$module" in
        opencv|opencv4|opencv5)
            case "$family" in
                arch)               echo "opencv" ;;
                debian)             echo "libopencv-dev" ;;
                fedora)             echo "opencv-devel" ;;
                suse)               echo "opencv-devel" ;;
                alpine)             echo "opencv-dev" ;;
                *)                  echo "" ;;
            esac
            ;;
        sdl2)
            case "$family" in
                arch)               echo "sdl2" ;;
                debian)             echo "libsdl2-dev" ;;
                fedora)             echo "SDL2-devel" ;;
                suse)               echo "SDL2-devel" ;;
                alpine)             echo "sdl2-dev" ;;
                *)                  echo "" ;;
            esac
            ;;
        libavformat)            dep_pkg "libavformat-dev" ;;
        libavcodec)             dep_pkg "libavcodec-dev" ;;
        libavutil)              dep_pkg "libavutil-dev" ;;
        libswresample)          dep_pkg "libswresample-dev" ;;
        *)                      echo "" ;;
    esac
}

# Mapping FFmpeg to linux family
dep_pkg() {
    local debian_pkg="$1"
    case "$family" in
        arch)               echo "ffmpeg" ;;
        debian)             echo "$debian_pkg" ;;
        fedora)             echo "ffmpeg-devel" ;;
        suse)               echo "ffmpeg-devel" ;;
        alpine)             echo "ffmpeg-dev" ;;
        *)                  echo "" ;;
    esac
}

# check for pkg-config
if ! command -v "$PKG_CONFIG" >/dev/null 2>&1; then
    echo "ERROR: '$PKG_CONFIG' not found (pkg-config / pkgconf)." >&2
    echo "Install  pkg-config:" >&2
    case "$family" in
        arch)   echo "  sudo pacman -S --needed pkgconf" >&2 ;;
        debian) echo "  sudo apt install pkg-config" >&2 ;;
        fedora) echo "  sudo dnf install pkgconf-pkg-config" >&2 ;;
        suse)   echo "  sudo zypper install pkgconf-pkg-config" >&2 ;;
        alpine) echo "  sudo apk add pkgconf" >&2 ;;
        *)      echo "  (install 'pkg-config' or 'pkgconf')" >&2 ;;
    esac
    exit 1
fi

# check for missing modules 
missing_modules=()
missing_pkgs=()
ok_count=0

add_pkg() {
    local pkg="$1"
    [ -n "$pkg" ] || return 0
    local existing
    for existing in "${missing_pkgs[@]:-}"; do
        [ "$existing" = "$pkg" ] && return 0
    done
    missing_pkgs+=("$pkg")
}

for module in "${modules[@]}"; do
    if [ "$module" = "opencv" ]; then
        # OpenCV 5 (Arch) albo 4 (Debian/Fedora).
        opencv_found=""
        for candidate in opencv5 opencv4; do
            if "$PKG_CONFIG" --exists "$candidate"; then
                version="$("$PKG_CONFIG" --modversion "$candidate" 2>/dev/null)"
                printf '[ok]   %-14s %s\n' "$candidate" "$version"
                opencv_found="$candidate"
                break
            fi
        done
        if [ -n "$opencv_found" ]; then
            ok_count=$((ok_count + 1))
        else
            printf '[missing] %-14s\n' "opencv (opencv5/opencv4)"
            missing_modules+=("opencv (opencv5/opencv4)")
            add_pkg "$(pkg_for_module opencv)"
        fi
        continue
    fi

    if "$PKG_CONFIG" --exists "$module"; then
        version="$("$PKG_CONFIG" --modversion "$module" 2>/dev/null)"
        printf '[ok]   %-14s %s\n' "$module" "$version"
        ok_count=$((ok_count + 1))
    else
        printf '[missing] %-14s\n' "$module"
        missing_modules+=("$module")
        add_pkg "$(pkg_for_module "$module")"
    fi
done

# summary
if [ "${#missing_modules[@]}" -eq 0 ]; then
    echo
    echo "All dependencies ($ok_count/${#modules[@]}) available."
    exit 0
fi

echo
echo "Missing ${#missing_modules[@]} from ${#modules[@]} dependencies: ${missing_modules[*]}"

if [ "${#missing_pkgs[@]}" -eq 0 ]; then
    echo "Unknown distribution ($distro ${distro_like}). Install missing modules:"
    echo "  ${missing_modules[*]}"
    exit 1
fi

echo
case "$family" in
    arch)   echo "Your family distribution: Arch. Install:";   echo "  sudo pacman -S --needed ${missing_pkgs[*]}" ;;
    debian) echo "Your family distribution: Debian/Ubuntu. Install:"; echo "  sudo apt install ${missing_pkgs[*]}" ;;
    fedora) echo "Your family distribution: Fedora/RHEL. Install:";   echo "  sudo dnf install ${missing_pkgs[*]}" ;;
    suse)   echo "Your family distribution: openSUSE. Install:";      echo "  sudo zypper install ${missing_pkgs[*]}" ;;
    alpine) echo "Your family distribution: Alpine. Install:";        echo "  sudo apk add ${missing_pkgs[*]}" ;;
esac

exit 1
