$paths = @('J:\cam_jf\cam_mon_cpp\src','J:\cam_jf\cam_mon_cpp\examples')
Get-ChildItem -Path $paths -Include *.cpp,*.h -Recurse | ForEach-Object {
    $p = $_.FullName
    try {
        $bytes = [System.IO.File]::ReadAllBytes($p)
        $sUtf8 = [System.Text.Encoding]::UTF8.GetString($bytes)
        if ($sUtf8.Contains([char]0xFFFD)) {
            # invalid UTF8 (contains replacement char), decode as cp936
            $s = [System.Text.Encoding]::GetEncoding(936).GetString($bytes)
            Write-Output "Detected cp936 for: $p"
        } else {
            $s = $sUtf8
            Write-Output "Detected UTF8 for: $p"
        }
        $out = [System.Text.Encoding]::UTF8.GetPreamble() + [System.Text.Encoding]::UTF8.GetBytes($s)
        [System.IO.File]::WriteAllBytes($p, $out)
        Write-Output "Wrote UTF8 BOM: $p"
    } catch {
        Write-Output "Failed: $p -> $_"
    }
}
