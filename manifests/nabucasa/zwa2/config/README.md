# ZWA-2 signing keys

`keys` is a symlink to `src/zwa2_controller/keys`, the canonical copy of the
ZWA-2 vendor keys.

The bootloader manifest pulls the keys in from here via its `config_file`
section. It cannot use the controller's approach of listing them in the base
project's slcp, because its base project `src/bootloader` is shared with the
other boards' bootloader manifests.
