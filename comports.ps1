# Lists the COM ports that are present right now and guesses what is on each
# one. build.cmd / build.sh run it after a no-argument compile so you can see
# which port to pass to --upload -p COMx.
# Read-only: it never opens a port (opening a Tab5's port resets the Tab5).
# Keep this file ASCII: Windows PowerShell 5.1 reads BOM-less scripts as ANSI.

$known = @{
    '303A:1001' = 'ESP32 USB-Serial/JTAG (Tab5, CoreS3 ...)'
    '1A86:55D4' = 'CH9102 USB-UART (Core2, Basic ...)'
    '1A86:55D3' = 'CH343 USB-UART'
    '1A86:7523' = 'CH340 USB-UART'
    '10C4:EA60' = 'CP210x USB-UART (Core2 v1.0, Gray ...)'
    '0403:6001' = 'FTDI USB-UART'
    '0403:6015' = 'FTDI USB-UART'
}

$rows = foreach ($d in Get-PnpDevice -Class Ports -PresentOnly -ErrorAction SilentlyContinue) {
    $id = $d.InstanceId
    $port = (Get-ItemProperty -LiteralPath "HKLM:\SYSTEM\CurrentControlSet\Enum\$id\Device Parameters" `
                 -ErrorAction SilentlyContinue).PortName
    if (-not $port -and $d.FriendlyName -match '\((COM\d+)\)') { $port = $Matches[1] }
    if ($port -notmatch '^COM(\d+)$') { continue }   # LPT etc.
    $num = [int]$Matches[1]

    $what = ''
    $info = ''
    if ($id -match '^USB\\VID_([0-9A-F]{4})&PID_([0-9A-F]{4})') {
        $vidpid = "$($Matches[1]):$($Matches[2])"
        $what = $known[$vidpid]
        if (-not $what -and $Matches[1] -eq '303A') { $what = 'ESP32 native USB' }
        if (-not $what) { $what = 'USB serial' }
        # The serial number is the last part of the USB device's instance ID
        # (for a composite device, the MI_xx interface's parent). On an ESP32
        # USB-Serial/JTAG it is the MAC, which tells two Tab5s apart. Windows
        # makes up an ID containing '&' when the device has no serial number.
        $usbDev = $id
        if ($id -match '&MI_') {
            $usbDev = (Get-PnpDeviceProperty -InstanceId $id -KeyName DEVPKEY_Device_Parent `
                           -ErrorAction SilentlyContinue).Data
        }
        $info = $vidpid
        $sn = "$usbDev".Split('\')[-1]
        if ($sn -and $sn -notmatch '&') { $info += "  SN $sn" }
    } elseif ($id -match '^BTHENUM\\') {
        $what = 'Bluetooth SPP'
        if ($id -match '&([0-9A-F]{12})_[0-9A-F]+$' -and $Matches[1] -ne '000000000000') {
            $addr = $Matches[1]
            $peer = Get-PnpDevice -Class Bluetooth -ErrorAction SilentlyContinue |
                    Where-Object { $_.InstanceId -like "BTHENUM\DEV_$addr\*" } | Select-Object -First 1
            $info = 'outgoing -> ' + (($addr -split '(..)' -ne '') -join ':')
            if ($peer) { $info += " ($($peer.FriendlyName))" }
        } else {
            $info = 'incoming'
        }
    } else {
        $what = 'other'
    }
    $name = $d.FriendlyName -replace '\s*\(COM\d+\)$', ''
    [pscustomobject]@{ Num = $num; Port = $port; What = $what; Info = $info; Name = $name }
}

$lines = @('', '--- COM ports ---')
if (-not $rows) {
    $lines += '  (none)'
} else {
    foreach ($r in $rows | Sort-Object Num) {
        $lines += '  {0,-6} {1,-40} {2}' -f $r.Port, $r.What, $r.Info
        $lines += '         {0}' -f $r.Name
    }
}
$text = ($lines -join [Environment]::NewLine) + [Environment]::NewLine

if ([Console]::IsOutputRedirected) {
    # Git Bash (mintty) and pipes expect UTF-8; write raw bytes so the Japanese
    # device names survive without touching the console code page.
    $bytes = (New-Object System.Text.UTF8Encoding $false).GetBytes($text)
    $out = [Console]::OpenStandardOutput()
    $out.Write($bytes, 0, $bytes.Length)
    $out.Flush()
} else {
    Write-Host -NoNewline $text
}
