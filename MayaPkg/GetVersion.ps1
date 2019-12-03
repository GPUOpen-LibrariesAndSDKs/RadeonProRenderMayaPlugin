$targetPath = "..\version.h"

if (-not (Test-Path $targetPath)) {
	throw "Version file not found."
}

$content = (Get-Content -Path $targetPath).trim()

$mask = '\s*?#define\s*PLUGIN_VERSION\s*"(?<Version>\d+?.\d+?.\d+?)"'

if (-not ($content -match $mask)) {
	throw "Version string not found."
}

return $Matches.Version
