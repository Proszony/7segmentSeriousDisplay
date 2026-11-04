#!/usr/bin/env bash
# Sprawdza zależności uart_app przez pkg-config i wypisuje gotowe polecenie
# instalacji dla wykrytej dystrybucji. Niczego nie instaluje.
#
# Użycie:
#   scripts/check-deps.sh [moduły pkg-config...]
#
# Zmienne:
#   PKG_CONFIG  - nazwa binarki (domyślnie pkg-config, akceptuje pkgconf)
#   OPENCV_PC   - moduł OpenCV (domyślnie opencv5); Makefile przekazuje wykryty

set -u

PKG_CONFIG="${PKG_CONFIG:-pkg-config}"
OPENCV_PC="${OPENCV_PC:-opencv5}"

if [ "$#" -gt 0 ]; then
    modules=("$@")
else
    modules=("$OPENCV_PC" sdl2 libavformat libavcodec libavutil libswresample)
fi

# ---------------------------------------------------------------- dystrybucja
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

# --------------------------------------------------- moduł -> pakiet(y) nazwy
pkg_for_module() {
    local module="$1"
    case "$module" in
        opencv4|opencv5)
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

# Wspólne mapowanie modułów FFmpeg (Debian ma osobne pakiety, reszta jedno).
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

# --------------------------------------------------------------- pkg-config?
if ! command -v "$PKG_CONFIG" >/dev/null 2>&1; then
    echo "BŁĄD: nie znaleziono '$PKG_CONFIG' (pkg-config / pkgconf)." >&2
    echo "Zainstaluj najpierw narzędzie pkg-config:" >&2
    case "$family" in
        arch)   echo "  sudo pacman -S --needed pkgconf" >&2 ;;
        debian) echo "  sudo apt install pkg-config" >&2 ;;
        fedora) echo "  sudo dnf install pkgconf-pkg-config" >&2 ;;
        suse)   echo "  sudo zypper install pkgconf-pkg-config" >&2 ;;
        alpine) echo "  sudo apk add pkgconf" >&2 ;;
        *)      echo "  (zainstaluj pakiet 'pkg-config' lub 'pkgconf')" >&2 ;;
    esac
    exit 1
fi

# ------------------------------------------------------------------- sprawdzanie
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
    if version="$("$PKG_CONFIG" --modversion "$module" 2>/dev/null)"; then
        printf '[ok]   %-14s %s\n' "$module" "$version"
        ok_count=$((ok_count + 1))
    else
        printf '[brak] %-14s\n' "$module"
        missing_modules+=("$module")
        add_pkg "$(pkg_for_module "$module")"
    fi
done

# ------------------------------------------------------------------- podsumowanie
if [ "${#missing_modules[@]}" -eq 0 ]; then
    echo
    echo "Wszystkie zależności ($ok_count/${#modules[@]}) są dostępne."
    exit 0
fi

echo
echo "Brakuje ${#missing_modules[@]} z ${#modules[@]} zależności: ${missing_modules[*]}"

if [ "${#missing_pkgs[@]}" -eq 0 ]; then
    echo "Nie rozpoznano dystrybucji ($distro ${distro_like}). Znajdź pakiety dostarczające:"
    echo "  ${missing_modules[*]}"
    exit 1
fi

echo
case "$family" in
    arch)   echo "Twoja dystrybucja: Arch. Zainstaluj:";   echo "  sudo pacman -S --needed ${missing_pkgs[*]}" ;;
    debian) echo "Twoja dystrybucja: Debian/Ubuntu. Zainstaluj:"; echo "  sudo apt install ${missing_pkgs[*]}" ;;
    fedora) echo "Twoja dystrybucja: Fedora/RHEL. Zainstaluj:";   echo "  sudo dnf install ${missing_pkgs[*]}" ;;
    suse)   echo "Twoja dystrybucja: openSUSE. Zainstaluj:";      echo "  sudo zypper install ${missing_pkgs[*]}" ;;
    alpine) echo "Twoja dystrybucja: Alpine. Zainstaluj:";        echo "  sudo apk add ${missing_pkgs[*]}" ;;
esac

exit 1
