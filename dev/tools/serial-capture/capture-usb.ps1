param([string]$Port = 'COM11', [string]$Out)
# For a TinyUSB CDC port: the port disappears on reset, so keep retrying to open it.
$w = New-Object System.IO.StreamWriter($Out, $true, (New-Object System.Text.UTF8Encoding $false))
$w.AutoFlush = $true
$w.WriteLine("# capture start $(Get-Date -Format o) port=$Port (reconnecting)")
while ($true) {
  $p = New-Object System.IO.Ports.SerialPort $Port, 115200
  $p.DtrEnable = $true; $p.RtsEnable = $true
  $p.ReadTimeout = 1000; $p.NewLine = "`n"
  try { $p.Open() } catch { Start-Sleep -Milliseconds 300; continue }
  $w.WriteLine("# port opened $(Get-Date -Format 'HH:mm:ss.fff')")
  while ($true) {
    try { $line = $p.ReadLine().TrimEnd("`r") }
    catch [System.TimeoutException] { continue }
    catch { $w.WriteLine("# port lost $(Get-Date -Format 'HH:mm:ss.fff')"); break }
    $w.WriteLine("[$(Get-Date -Format 'HH:mm:ss.fff')] $line")
  }
  try { $p.Close() } catch {}
  $p.Dispose()
  Start-Sleep -Milliseconds 300
}
