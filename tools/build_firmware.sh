#!/bin/bash
set -euo pipefail

build_dir="build_dir"
output_dir="outputs"
work_dir="/build"
manifest_files=()
python_interpreter="/opt/venv/bin/python3"

# Show help message
show_help() {
	echo "Usage: $0 [OPTIONS]"
	echo
	echo "Build firmware images from manifest files"
	echo
	echo "Options:"
	echo "  -h, --help                 Show this help message"
	echo "  -m, --manifest FILE        YAML manifest file describing the firmware to build"
	echo "                             (can be specified multiple times)"
	echo "  -b, --build-dir DIR        Directory for build files (default: ${build_dir})"
	echo "  -o, --output-dir DIR       Directory for output files (default: ${output_dir})"
	echo "  -w, --work-dir DIR         Working directory for build process (default: ${work_dir})"
	echo "  -p, --python PATH          Python interpreter path (default: ${python_interpreter})"
	exit 0
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
	case $1 in
	-h | --help)
		show_help
		;;
	-m | --manifest)
		if [ -z "${2:-}" ]; then
			echo "Error: --manifest requires a file argument"
			exit 1
		fi
		manifest_files+=("$2")
		shift
		;;
	-b | --build-dir)
		if [ -z "${2:-}" ]; then
			echo "Error: --build-dir requires a directory argument"
			exit 1
		fi
		build_dir="$2"
		shift
		;;
	-o | --output-dir)
		if [ -z "${2:-}" ]; then
			echo "Error: --output-dir requires a directory argument"
			exit 1
		fi
		output_dir="$2"
		shift
		;;
	-w | --work-dir)
		if [ -z "${2:-}" ]; then
			echo "Error: --work-dir requires a directory argument"
			exit 1
		fi
		work_dir="$2"
		shift
		;;
	-p | --python)
		if [ -z "${2:-}" ]; then
			echo "Error: --python requires a path argument"
			exit 1
		fi
		python_interpreter="$2"
		shift
		;;
	*)
		echo "Error: Unknown argument: $1"
		echo "Use -h or --help for usage information"
		exit 1
		;;
	esac
	shift
done

# Check that Python3 with ruamel.yaml is available
if ! "$python_interpreter" -c "import ruamel.yaml" &>/dev/null; then
	echo "Error: Python3 with ruamel.yaml is not available at $python_interpreter"
	echo "Install ruamel.yaml with 'pip install ruamel.yaml'"
	exit 1
fi

# Check if any manifest files were provided
if [ ${#manifest_files[@]} -eq 0 ]; then
	echo "Error: No manifest files provided"
	echo "Use -h or --help for usage information"
	exit 1
fi

git config --global --add safe.directory "$work_dir"

# Install SDK extensions
for sdk in /*_sdk_*; do
	slc signature trust --sdk "$sdk"
	ln -s "$(pwd)"/gecko_sdk_extensions "$sdk"/extension
	for ext in "$sdk"/extension/*/; do
		slc signature trust --sdk "$sdk" --extension-path "$ext"
	done
done

# Build SDK arguments
sdk_args=()
for sdk_dir in /*_sdk*; do
	sdk_args+=(--sdk "$sdk_dir")
done

# Build toolchain arguments
toolchain_args=()
for toolchain_dir in /opt/*arm-none-eabi*; do
	toolchain_args+=(--toolchain "$toolchain_dir")
done

# Determine if we should keep the SLC daemon running
keep_daemon=""
if [ ${#manifest_files[@]} -gt 1 ]; then
	keep_daemon="--keep-slc-daemon"
fi

# Build firmware
for manifest_file in "${manifest_files[@]}"; do
	echo "Building firmware manifest $manifest_file, using build directory $build_dir and output directory $output_dir"

	"$python_interpreter" tools/build_project.py \
		"${sdk_args[@]}" \
		"${toolchain_args[@]}" \
		$keep_daemon \
		--manifest "$manifest_file" \
		--build-dir "$build_dir" \
		--output-dir "$output_dir" \
		--build-system makefile \
		--output gbl \
		--output hex \
		--output out
done
