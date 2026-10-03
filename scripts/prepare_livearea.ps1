param(
    [string]$SourceDir = (Join-Path $PSScriptRoot '..\extras\livearea\source')
)
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath $SourceDir).Path
$output = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\extras\livearea')).Path
$preview = Join-Path $PSScriptRoot '..\analysis\livearea'
New-Item -ItemType Directory -Force -Path $preview | Out-Null

function Invoke-Magick([string[]]$Arguments) {
    & magick @Arguments
    if ($LASTEXITCODE -ne 0) { throw 'ImageMagick asset conversion failed.' }
}

# Keep the supplied icon and lettering intact; resize and encode for the shell.
$scene = Join-Path $source 'scene.png'
$logo = Join-Path $source 'logo.webp'
$palette = @('-strip', '-colorspace', 'sRGB', '-alpha', 'off', '-dither', 'FloydSteinberg', '-colors', '128', '+dither', '-depth', '8')
Invoke-Magick (@((Join-Path $source 'icon.png'), '-filter', 'Lanczos', '-resize', '128x128!', '-alpha', 'off') + $palette + @('PNG8:' + (Join-Path $output 'icon0.png')))

# Style a1 reserves x=280..560, y=139..297 of the 840x500 background for the gate.
# The original logo fits entirely above it; Maxwell remains clear on the right.
Invoke-Magick (@($scene, '-resize', '840x500^', '-gravity', 'center', '-extent', '840x500',
    '(', $logo, '-resize', '350x128', ')', '-gravity', 'north', '-geometry', '+0+5', '-compose', 'over', '-composite') +
    $palette + @('PNG8:' + (Join-Path $output 'bg0.png')))

Invoke-Magick (@($scene, '-resize', '960x544^', '-gravity', 'center', '-extent', '960x544',
    '(', $logo, '-resize', '440x162', ')', '-gravity', 'north', '-geometry', '+0+12', '-compose', 'over', '-composite') +
    $palette + @('PNG8:' + (Join-Path $output 'pic0.png')))

Invoke-Magick (@((Join-Path $source 'launch.webp'), '-resize', '280x158^', '-gravity', 'center', '-extent', '280x158') +
    $palette + @('PNG8:' + (Join-Path $output 'startup.png')))

# Layout preview only: the shell supplies its own Start button and gate border.
Invoke-Magick @((Join-Path $output 'bg0.png'), (Join-Path $output 'startup.png'),
    '-gravity', 'northwest', '-geometry', '+280+139', '-compose', 'over', '-composite',
    '-fill', 'none', '-stroke', '#edf3ed', '-strokewidth', '3', '-draw', 'roundrectangle 280,139 560,297 8,8',
    '-stroke', 'none', '-fill', '#14a8de', '-draw', 'roundrectangle 360,257 480,289 10,10',
    '-font', 'Arial-Bold', '-pointsize', '16', '-fill', 'white', '-gravity', 'northwest', '-annotate', '+400+261', 'Start',
    (Join-Path $preview 'livearea-preview.png'))
Write-Output "Prepared LiveArea assets in $output"
Write-Output "Layout preview: $preview\livearea-preview.png"
