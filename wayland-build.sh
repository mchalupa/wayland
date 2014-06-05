#!/bin/sh

## wayland-build.sh
#  a script for building and updating Wayland and Weston
#  from git repositories.

WAYLAND_GIT="git://anongit.freedesktop.org/wayland/wayland"
DRM_GIT="git://anongit.freedesktop.org/git/mesa/drm"
MESA_GIT="git://anongit.freedesktop.org/mesa/mesa"
LIBXKBCOMMON_GIT="git://github.com/xkbcommon/libxkbcommon"
PIXMAN_GIT="git://anongit.freedesktop.org/pixman"
CAIRO_GIT="git://anongit.freedesktop.org/cairo"
LIBUNWIND_GIT="git://git.sv.gnu.org/libunwind"
WESTON_GIT="git://anongit.freedesktop.org/wayland/weston"
LIBFFI_GIT="git://github.com/atgreen/libffi.git"
PTHREAD_STUBS_GIT="git://anongit.freedesktop.org/xcb/pthread-stubs"
PCIACCESS_GIT="git://anongit.freedesktop.org/xorg/lib/libpciaccess"
XORG_MACROS_GIT="http://cgit.freedesktop.org/xorg/util/macros/"
GLPROTO_GIT="git://anongit.freedesktop.org/xorg/proto/glproto"
DRI2PROTO_GIT="git://anongit.freedesktop.org/xorg/proto/dri2proto"
DRI3PROTO_GIT="git://anongit.freedesktop.org/xorg/proto/dri3proto"
PRESENTPROTO_GIT="git://anongit.freedesktop.org/xorg/proto/presentproto"
XPROTO_GIT="git://anongit.freedesktop.org/xorg/proto/xproto"
XEXTPROTO_GIT="git://anongit.freedesktop.org/xorg/proto/xextproto"
LIBXTRANS_GIT="git://anongit.freedesktop.org/xorg/lib/libxtrans"
LIBX11_GIT="git://anongit.freedesktop.org/xorg/lib/libX11"
LIBXEXT_GIT="git://anongit.freedesktop.org/xorg/lib/libXext"
LIBXDAMAGE_GIT="git://anongit.freedesktop.org/xorg/lib/libXdamage"
LIBXFIXES_GIT="git://anongit.freedesktop.org/xorg/lib/libXfixes"

LIBXSHMFENCE_GIT="git://anongit.freedesktop.org/xorg/lib/libxshmfence"

# TODO delete empty
WAYLAND_FLAGS=
MESA_FLAGS='--enable-gles2 --disable-gallium-egl --with-egl-platforms=x11,wayland,drm --enable-shared-glapi --with-gallium-drivers=r300,r600,swrast,nouveau --enable-gbm'
LIBXKBCOMMON_FLAGS='--with-xkb-config-root=/usr/share/X11/xkb'
CAIRO_FLAGS='--enable-gl --enable-xcb'

WAYLAND_CFLAGS=

if ! which doxygen 2>/dev/null; then
	WAYLAND_FLAGS="$WAYLAND_FLAGS --disable-documentation"
fi

ROOT_SHELL_RUNNING="0"

function err()
{
	echo "$1" 1>&2
	if [ "$ROOT_SHELL_RUNNING" = "1" ]; then
		echo "exit" >&7 # kill root shell
	fi
	exit 1
}

# setup environment
function env()
{
	INITIALIZED=1
	WLD=`cat .wayland-build.cfg 2>/dev/null`

	if [ -z "$WLD" ]; then
		echo -n "Installation prefix (/usr/local): "
		read WLD
		if [ -z "$WLD" ]; then
			WLD="/usr/local"
		fi
		echo "$WLD" > ".wayland-build.cfg"
		INITIALIZED=0
		echo "Using prefix: $WLD"
	fi

	LD_LIBRARY_PATH="$WLD/lib"
	PKG_CONFIG_PATH="$WLD/lib/pkgconfig:$WLD/share/pkgconfig"
	ACLOCAL_PATH="$WLD/share/aclocal"
	ACLOCAL="aclocal -I $ACLOCAL_PATH"
	PATH="$WLD/bin:$PATH"

	export WLD LD_LIBRARY_PATH PKG_CONFIG_PATH
	export ACLOCAL_PATH ACLOCAL PATH

	if test -z "$XDG_RUNTIME_DIR"; then
		XDG_RUNTIME_DIR=/tmp/${UID}-runtime-dir
		mkdir -p "$XDG_RUNTIME_DIR"
		chmod 0700 "$XDG_RUNTIME_DIR"
		export XDG_RUNTIME_DIR
	fi

	return $INITIALIZED
}

function clone()
{
	if [ ! -d $2 ]; then
		git clone "$1" "$2" || err "Failed downloading from $1"
	fi
}

function check_module()
{
	pkg-config --exists "$1"
	return $?
}

function init()
{
	COMPONENT="$1"
	echo "Downloading sources for ${COMPONENT}..."

	case $COMPONENT in
	"wayland")
		clone "$WAYLAND_GIT" "wayland"
		;;
	"weston") if [ ! -d wayland ]; then
			err "Build wayland first ($0 all wayland)"
		fi

		clone "$DRM_GIT" "drm"
		clone "$MESA_GIT" "mesa"
		clone "$LIBXKBCOMMON_GIT" "libxkbcommon"
		clone "$WESTON_GIT" "weston"

		;;
	esac
}

function run_root_shell()
{
	if [ "$ROOT_SHELL_RUNNING" = "1" ]; then
		err "Root shell already running"
	fi

	PIPE=$(mktemp -u)
	exec 7>"$PIPE"
	CMD="( exec <${PIPE}; while true; do eval \"\$(cat <&0)\"; done )&"
	su -c "$CMD" || { rm $PIPE; err "Needs root privileges"; }
	rm $PIPE

	ROOT_SHELL_RUNNING=1
}

function run_root()
{
	echo "$1" >&7;
	echo "" >&7; # enter
}

function install_module()
{
	MODULE="$1"
	echo "Installing ${MODULE}"

	cd "$1"

	if [ "$UID" != "0" -a "$ROOT_SHELL_RUNNING" != 1 ]; then
		echo "Need root privileges for installing"
		# so that we don't have to
		# ask for permission all the time
		run_root_shell
	fi

	run_root "make install $CFLAGS"

	cd -
}

function build_module()
{
	MODULE="$1"
	echo "Building module $1..."

	if [ "$MODULE" = "mesa" ]; then
		NOT=1

		flex --version 2>&1 1>/dev/null\
		|| { NOT=0; echo "You need install flex program" >&2; }

		bison --version 2>&1 1>/dev/null\
		|| { NOT=0; echo "You need install bison program" >&2; }

		if [ "$NOT" = "0" ]; then
			exit 1;
		fi
	fi

	cd "$MODULE" || err "No directory named $1"

	MOD_VAR=$(echo $MODULE | tr '[:lower:]' '[:upper:]')
	MOD_VAR=$(echo $MOD_VAR | tr '-' '_')
	FLAGS="$(eval echo \$${MOD_VAR}_FLAGS)"
	AUTOGEN="./autogen.sh --prefix=$WLD $FLAGS"

	if [ ! -f "Makefile" ]; then
		$AUTOGEN || err "Autogen for $MODULE failed"
	fi

	# autogen.sh must not call ./configure
	if [ ! -f "config.log" ]; then
		./configure "--prefix=$WLD" "$FLAGS" \
		|| err "Configure failed"
	fi

	make CFLAGS="$(eval \$${MOD_VAR}_CFLAGS) $CFLAGS" || err "make failed"

	cd -
}

function check_and_build_module()
{
	MODULE="$1"
	MOD_VAR=$(echo $MODULE | tr '[:lower:]' '[:upper:]')
	MOD_VAR=$(echo $MOD_VAR | tr '-' '_')
	GIT="$(eval echo \$${MOD_VAR}_GIT)"

	if ! check_module "$MODULE"; then
		echo ""
		echo "Did not find $MODULE, should I build it from git? [y]"
		echo "(If not, the building will continue, but probably will fail)"
		read ANSWER
		if [ "x$ANSWER" = "xy" ]; then
			if [ ! -d "$MODULE" ]; then
				clone "$GIT" "$MODULE"
			fi

			build_module "$MODULE"
			install_module "$MODULE"
		fi
	fi
}

function build()
{
	COMPONENT="$1"
	echo "Building ${COMPONENT}..."

	case $COMPONENT in
	"wayland")
		if [ ! -d "wayland" ]; then
			err "First download Wayland ($0 init wayland)"
		fi

		check_and_build_module "libffi"

		build_module "wayland"
		;;
	"weston")
		if [ ! -d "weston" ]; then
			err "First download Weston ($0 init weston)"
		fi

		check_and_build_module "pthread-stubs"
		check_and_build_module "xorg-macros"
		check_and_build_module "pciaccess"
		build_module "drm"
 		# need install because it's prereq. for mesa
		if ! check_module "libdrm"; then
			install_module "drm"
		fi

		check_and_build_module "glproto"
		check_and_build_module "dri2proto"
		check_and_build_module "dri3proto"
		check_and_build_module "presentproto"
		check_and_build_module "libxtrans"
		check_and_build_module "xproto"
		check_and_build_module "libx11"
		check_and_build_module "xextproto"
		check_and_build_module "libxext"
		check_and_build_module "libxdamage"
		check_and_build_module "libxfixes"
		build_module "mesa"

		build_module "libxkbcommon"
		build_module "cairo"
		build_module "libunwind"
		build_module "weston"
		;;
	esac
}

if [ -z "$1" ]; then
	#empty directory
	if env; then
		init "wayland"
		build "wayland"
		exit 0
	else
		ACTION="build"
	fi
else
	ACTION="$1"
fi

if [ -z "$2" ]; then
	COMPONENT="wayland"
else
	# can be weston or wayland (default)
	COMPONENT="$2"
fi

env

case "$ACTION" in
# build all dependencies and wayland/weston
"build")
	build "$COMPONENT"
	;;
# do git pull in all deps and wayland
"update")
	echo "Updating ${COMPONENT}..."
	;;
"init")
	init "$COMPONENT"
	;;
"install")
	install_module "wayland"
	;;
"all") # automated way to do all the work
	echo "Updating and rebuilding ${COMPONENT}"
	;;
"env")
	env
	echo "Running shell with Wayland environment..."
	$SHELL -c "export PS1=\"[Wld] $PS1\"; exec $SHELL"
	echo "Exiting shell with Wayland environment..."
	;;
*)
	echo "Help!";;
esac

if [ "$ROOT_SHELL_RUNNING" = "1" ]; then
	echo "exit">&7
fi
