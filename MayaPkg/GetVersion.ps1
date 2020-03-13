# Copyright 2020 Advanced Micro Devices, Inc
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#     http://www.apache.org/licenses/LICENSE-2.0
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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
