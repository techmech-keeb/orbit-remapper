param([string]$Port = 'COM9', [string]$Out, [switch]$Dtr)
$p = New-Object System.IO.Ports.SerialPort $Port, 115200
# USB Serial/JTAG (Q31): DTR off so opening the port does not reset the board.
# TinyUSB CDC (M1): pass -Dtr, since the firmware may only write when DTR is asserted.
$p.DtrEnable = [bool]$Dtr; $p.RtsEnable = [bool]$Dtr
$p.ReadTimeout = 1000; $p.NewLine = "`n"
$p.Open()
$w = New-Object System.IO.StreamWriter($Out, $true, (New-Object System.Text.UTF8Encoding $false))
$w.AutoFlush = $true
$w.WriteLine("# capture start $(Get-Date -Format o) port=$Port dtr=$([bool]$Dtr)")
while ($true) {
  try { $line = $p.ReadLine().TrimEnd("`r") } catch [System.TimeoutException] { continue }
  $w.WriteLine("[$(Get-Date -Format 'HH:mm:ss.fff')] $line")
}
