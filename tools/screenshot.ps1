# Captures the primary screen to a PNG (for inspecting game state headlessly).
param([Parameter(Mandatory = $true)][string] $OutFile)
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
$b = [System.Windows.Forms.SystemInformation]::VirtualScreen
$bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($b.Left, $b.Top, 0, 0, $bmp.Size)
# Downscale to keep file size manageable for review
$scale = 0.5
$small = New-Object System.Drawing.Bitmap ([int]($b.Width * $scale)), ([int]($b.Height * $scale))
$g2 = [System.Drawing.Graphics]::FromImage($small)
$g2.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::Bilinear
$g2.DrawImage($bmp, 0, 0, $small.Width, $small.Height)
$small.Save($OutFile, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $g2.Dispose(); $bmp.Dispose(); $small.Dispose()
Write-Host "saved $OutFile"
