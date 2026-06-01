$paths = @('J:\cam_jf\cam_mon_cpp\src','J:\cam_jf\cam_mon_cpp\examples')
Get-ChildItem -Path $paths -Include *.cpp,*.h -Recurse | ForEach-Object {
    $p = $_.FullName
    try {
        $bytes = [System.IO.File]::ReadAllBytes($p)
        $s = [System.Text.Encoding]::GetEncoding(936).GetString($bytes)
        $out = [System.Text.Encoding]::UTF8.GetPreamble() + [System.Text.Encoding]::UTF8.GetBytes($s)
        [System.IO.File]::WriteAllBytes($p, $out)
        Write-Output "Converted: $p"
    } catch {
        Write-Output "Failed: $p -> $_"
    }
}
