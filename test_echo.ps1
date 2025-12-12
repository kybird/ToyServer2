$port = 9000
$client = New-Object System.Net.Sockets.TcpClient("127.0.0.1", $port)
$stream = $client.GetStream()

# Construct Packet
# Header: Size(2) + ID(2)
# ID = 2000 (0x07D0) PKT_C_ECHO
# Payload = "Hello" (5 bytes)
# Total Size = 4 + 5 = 9

$payload = [System.Text.Encoding]::ASCII.GetBytes("Hello")
$size = [uint16]($payload.Length + 4)
$id = [uint16]2000

$headerBuffer = New-Object byte[] 4
[System.BitConverter]::GetBytes($size).CopyTo($headerBuffer, 0)
[System.BitConverter]::GetBytes($id).CopyTo($headerBuffer, 2)

$packet = $headerBuffer + $payload

# Send
Write-Host "Sending Echo Request (Size: $size, ID: $id)"
$stream.Write($packet, 0, $packet.Length)

# Receive Header
$headerRecv = New-Object byte[] 4
$bytesRead = $stream.Read($headerRecv, 0, 4)
if ($bytesRead -lt 4) {
    Write-Error "Failed to read header"
    exit 1
}

$recvSize = [System.BitConverter]::ToUInt16($headerRecv, 0)
$recvId = [System.BitConverter]::ToUInt16($headerRecv, 2)

Write-Host "Received Header (Size: $recvSize, ID: $recvId)"

# Receive Payload
$payloadLen = $recvSize - 4
$payloadRecv = New-Object byte[] $payloadLen
$bytesRead = $stream.Read($payloadRecv, 0, $payloadLen)

$recvMsg = [System.Text.Encoding]::ASCII.GetString($payloadRecv)
Write-Host "Received Payload: $recvMsg"

$client.Close()

if ($recvId -eq 2001 -and $recvMsg -eq "Hello") {
    Write-Host "TEST PASSED"
    exit 0
} else {
    Write-Error "TEST FAILED"
    exit 1
}
