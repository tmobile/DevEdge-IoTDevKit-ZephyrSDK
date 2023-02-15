if (!$env:ZEPHYR_EXTRA_MODULES) {
    $env:ZEPHYR_EXTRA_MODULES = $PSScriptRoot
} else {
    $env:ZEPHYR_EXTRA_MODULES += ';' + $PSScriptRoot
}
