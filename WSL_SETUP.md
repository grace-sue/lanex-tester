# WSL Mirrored Networking Setup

This guide explains how to run the LAN-EX tester from WSL 2 using mirrored networking on Windows.

The tester runs `iperf3` servers locally and tells the EdgeRouter to connect back to them over SSH. Because of that, the `serverIps.conf` addresses must be reachable from the EdgeRouter/LAN-EX network.

## Expected IP Layout

Use this layout unless your bench network is intentionally different:

| Pair | Windows/WSL tester IP | EdgeRouter/LAN-EX IP |
| ---- | --------------------- | -------------------- |
| 1 | `192.168.1.20` | `192.168.1.1` |
| 2 | `192.168.2.20` | `192.168.2.1` |
| 3 | `192.168.3.20` | `192.168.3.1` |
| 4 | `192.168.4.20` | `192.168.4.1` |
| 5 | `192.168.5.20` | `192.168.5.1` |
| 6 | `192.168.6.20` | `192.168.6.1` |
| 7 | `192.168.7.20` | `192.168.7.1` |
| 8 | `192.168.8.20` | `192.168.8.1` |

The project config files should match that layout:

```text
config/serverIps.conf
192.168.1.20
192.168.2.20
192.168.3.20
192.168.4.20
192.168.5.20
192.168.6.20
192.168.7.20
192.168.8.20
```

```text
config/clientIps.conf
192.168.1.1
192.168.2.1
192.168.3.1
192.168.4.1
192.168.5.1
192.168.6.1
192.168.7.1
192.168.8.1
```

Think of the files this way:

- `serverIps.conf`: IPs for the local tester machine, used by remote `iperf3` clients to connect back.
- `clientIps.conf`: IPs of the EdgeRouter interfaces that this app pings and SSHes into.

## Requirements

- Windows 11 22H2 or newer.
- WSL 2.
- WSL mirrored networking enabled.
- Windows Ethernet adapter connected to the LAN-EX/EdgeRouter test network.
- Inbound access allowed for TCP ports `5201-5208`.
- Passwordless SSH from WSL to each EdgeRouter interface.

Check WSL:

```powershell
wsl --version
wsl --status
```

## Enable WSL Mirrored Networking

On Windows, create or edit this file:

```text
C:\Users\<your-user>\.wslconfig
```

Add:

```ini
[wsl2]
networkingMode=mirrored
firewall=true

[experimental]
hostAddressLoopback=true
```

Restart WSL from PowerShell:

```powershell
wsl --shutdown
```

Open WSL again after shutdown completes.

Microsoft documentation:

- <https://learn.microsoft.com/en-us/windows/wsl/wsl-config>
- <https://learn.microsoft.com/en-us/windows/wsl/networking>

## Add Tester IPs To Windows

Run PowerShell as Administrator.

Find the Ethernet adapter name:

```powershell
Get-NetAdapter
```

If the adapter is named `Ethernet`, add the tester IPs:

```powershell
New-NetIPAddress -InterfaceAlias "Ethernet" -IPAddress 192.168.1.20 -PrefixLength 24
New-NetIPAddress -InterfaceAlias "Ethernet" -IPAddress 192.168.2.20 -PrefixLength 24
New-NetIPAddress -InterfaceAlias "Ethernet" -IPAddress 192.168.3.20 -PrefixLength 24
New-NetIPAddress -InterfaceAlias "Ethernet" -IPAddress 192.168.4.20 -PrefixLength 24
New-NetIPAddress -InterfaceAlias "Ethernet" -IPAddress 192.168.5.20 -PrefixLength 24
New-NetIPAddress -InterfaceAlias "Ethernet" -IPAddress 192.168.6.20 -PrefixLength 24
New-NetIPAddress -InterfaceAlias "Ethernet" -IPAddress 192.168.7.20 -PrefixLength 24
New-NetIPAddress -InterfaceAlias "Ethernet" -IPAddress 192.168.8.20 -PrefixLength 24
```

If your adapter has a different name, replace `"Ethernet"` with the name shown by `Get-NetAdapter`.

Verify:

```powershell
Get-NetIPAddress -InterfaceAlias "Ethernet" -AddressFamily IPv4
```

## Allow Inbound Traffic To WSL

The tester needs inbound TCP connections from the EdgeRouter to WSL on ports `5201-5208`.

Run PowerShell as Administrator and allow those ports through the Hyper-V firewall:

```powershell
New-NetFirewallHyperVRule -Name "LANEX-iperf" -DisplayName "LANEX iperf" -Direction Inbound -VMCreatorId "{40E0AC32-46A5-438A-A0B2-2B479E8F2E90}" -Protocol TCP -LocalPorts 5201-5208
```

If you are troubleshooting and want to allow all inbound WSL traffic temporarily:

```powershell
Set-NetFirewallHyperVVMSetting -Name "{40E0AC32-46A5-438A-A0B2-2B479E8F2E90}" -DefaultInboundAction Allow
```

Use the specific port rule for normal use.

## EdgeRouter Setup

Set the EdgeRouter/LAN-EX interfaces to match the client IPs:

```text
eth0 -> 192.168.1.1/24
eth1 -> 192.168.2.1/24
eth2 -> 192.168.3.1/24
eth3 -> 192.168.4.1/24
eth4 -> 192.168.5.1/24
eth5 -> 192.168.6.1/24
eth6 -> 192.168.7.1/24
eth7 -> 192.168.8.1/24
```

The exact interface names may differ depending on the router model and cabling. The important part is that each test pair has one `192.168.x.1` EdgeRouter side and one matching `192.168.x.20` tester side.

## SSH Setup From WSL

Inside WSL:

```bash
ssh-keygen -t rsa -b 4096
```

Leave the passphrase empty. This program cannot answer passphrase prompts.

This creates:

```text
~/.ssh/id_rsa
~/.ssh/id_rsa.pub
```

Copy the public key to the EdgeRouter. Use any reachable EdgeRouter interface. In this example, the first interface is used:

```bash
scp ~/.ssh/id_rsa.pub ubnt@192.168.1.1:/tmp/lanex-tester.pub
```

Log in to the EdgeRouter:

```bash
ssh ubnt@192.168.1.1
```

On the EdgeRouter CLI, load the key for the `ubnt` user:

```text
configure
loadkey ubnt /tmp/lanex-tester.pub
save
exit
exit
```

The first `exit` leaves EdgeRouter configuration mode. The second `exit` closes the SSH session.

If `loadkey` reports that it cannot find the file, confirm that the key was copied to the same path:

```bash
ls -l /tmp/lanex-tester.pub
```

If the router asks for a password during `scp` or the first `ssh`, enter the EdgeRouter `ubnt` user password. After `loadkey` and `save`, future SSH connections from WSL should use the key instead of a password.

Newer OpenSSH clients may need RSA/SHA-1 enabled for older EdgeRouter firmware. Add this to `~/.ssh/config` in WSL:

```sshconfig
Host 192.168.*.1
    User ubnt
    PubkeyAcceptedAlgorithms +ssh-rsa
    HostKeyAlgorithms +ssh-rsa
```

Then:

```bash
chmod 600 ~/.ssh/config
```

Verify passwordless SSH from WSL:

```bash
for i in 1 2 3 4 5 6 7 8; do ssh -o BatchMode=yes ubnt@192.168.$i.1 echo "OK $i"; done
```

Every line should print `OK <number>`.

## Verify Network Reachability

From WSL, ping each EdgeRouter interface:

```bash
for i in 1 2 3 4 5 6 7 8; do ping -c 1 -W2 192.168.$i.1; done
```

Optional: test one iperf path manually.

In WSL terminal 1:

```bash
iperf3 -s --one-off --port 5201
```

In WSL terminal 2, SSH to the EdgeRouter and run:

```bash
iperf3 -c 192.168.1.20 --port 5201 -t 5
```

If that works, the EdgeRouter can connect back to the WSL tester.

## Build And Run

Inside WSL:

```bash
sudo apt update
sudo apt install libncurses5 libncurses5-dev iperf3 make g++
make build
mkdir -p reports/eng
make run
```

## VLAN Notes

Mirrored networking mirrors Windows networking into WSL. It does not create VLAN interfaces by itself.

If the bench uses real 802.1Q tagged VLANs, Windows must expose those VLAN interfaces first. Common options are:

- Configure VLAN interfaces using the Windows NIC driver, then assign `192.168.x.20` addresses to those VLAN interfaces.
- Configure the switch/router ports as access/untagged ports so Windows only sees normal Ethernet networks.
- Use native Linux or a bridged Linux VM if VLAN tagging must be controlled from Linux.

For the simplest LAN-EX tester setup, avoid VLAN tagging on the WSL side. Let Windows own the `192.168.x.20` addresses and let WSL mirrored networking expose them to the Linux environment.

## Troubleshooting

If ping works but the test hangs on `tmp`, check SSH first:

```bash
ssh -o BatchMode=yes ubnt@192.168.1.1 echo OK
```

If SSH works but iperf does not update TX/RX, check inbound firewall access to ports `5201-5208`.

If WSL shows only `172.x.x.x` addresses, mirrored networking is probably not active. Recheck `.wslconfig`, run `wsl --shutdown`, and restart WSL.

If `New-NetIPAddress` says the address already exists, verify with:

```powershell
Get-NetIPAddress -AddressFamily IPv4 | Where-Object IPAddress -Like "192.168.*"
```

If the EdgeRouter cannot reach `192.168.x.20`, make sure the IP address was added to the correct Windows Ethernet adapter and that the physical cabling/switching matches the `192.168.x.1` network.
