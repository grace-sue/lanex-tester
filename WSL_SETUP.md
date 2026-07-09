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

## Network Goal

The Windows PC uses one physical Ethernet NIC connected to a switch.

WSL uses mirrored networking and accesses the same physical network through interface `eth1`.

The PC/WSL side uses these IP addresses:

```text
192.168.1.20/24
192.168.2.20/24
192.168.3.20/24
192.168.4.20/24
192.168.5.20/24
192.168.6.20/24
192.168.7.20/24
192.168.8.20/24
```

The router ports use:

```text
192.168.1.1/24
192.168.2.1/24
192.168.3.1/24
192.168.4.1/24
192.168.5.1/24
192.168.6.1/24
192.168.7.1/24
192.168.8.1/24
```

The topology is:

```text
WSL application
      |
WSL eth1
      |
Windows Ethernet NIC
      |
    Switch
      |
Router ports 1–8
```

No VLAN configuration is required. All switch ports can stay in the same untagged VLAN.

---

## Step 1: Enable WSL Mirrored Networking

Open PowerShell and edit the WSL configuration file:

```powershell
notepad $env:USERPROFILE\.wslconfig
```

Add:

```ini
[wsl2]
networkingMode=mirrored
dnsTunneling=true
firewall=true
autoProxy=true
```

Save the file.

Restart WSL:

```powershell
wsl --shutdown
```

Then open WSL again.

---

## Step 2: Confirm the Mirrored Interface

Inside WSL, run:

```bash
ip -br addr
```

The physical mirrored Ethernet interface should appear as:

```text
eth1
```

Check its details:

```bash
ip -4 addr show dev eth1
```

---

## Step 3: Add the Secondary IP Addresses in WSL

Run:

```bash
for i in {1..8}; do
    sudo ip addr add "192.168.$i.20/24" dev eth1
done
```

This adds the following IP addresses to `eth1`:

```text
192.168.1.20
192.168.2.20
192.168.3.20
192.168.4.20
192.168.5.20
192.168.6.20
192.168.7.20
192.168.8.20
```

If an address already exists, Linux may show:

```text
RTNETLINK answers: File exists
```

This only means that the address is already configured.

Verify the addresses:

```bash
ip -4 addr show dev eth1
```

---

## Step 4: Add Routes with the Correct Source IP

WSL may not automatically select the matching source IP for each subnet.

Add one route for each subnet and explicitly specify the preferred source IP:

```bash
for i in {1..8}; do
    sudo ip route replace \
        "192.168.$i.0/24" \
        dev eth1 \
        src "192.168.$i.20"
done
```

This creates routes such as:

```text
192.168.1.0/24 dev eth1 src 192.168.1.20
192.168.2.0/24 dev eth1 src 192.168.2.20
192.168.3.0/24 dev eth1 src 192.168.3.20
```

The `src` value tells Linux which local IP address to use when sending traffic to that subnet.

---

## Step 5: Verify Route Selection

Run:

```bash
for i in {1..8}; do
    ip route get "192.168.$i.1"
done
```

The output should look similar to:

```text
192.168.1.1 dev eth1 src 192.168.1.20
192.168.2.1 dev eth1 src 192.168.2.20
192.168.3.1 dev eth1 src 192.168.3.20
192.168.4.1 dev eth1 src 192.168.4.20
192.168.5.1 dev eth1 src 192.168.5.20
192.168.6.1 dev eth1 src 192.168.6.20
192.168.7.1 dev eth1 src 192.168.7.20
192.168.8.1 dev eth1 src 192.168.8.20
```

The important part is that every destination uses the matching `src` address.

---

## Step 6: Test Every Router Port

Run:

```bash
for i in {1..8}; do
    echo "Testing 192.168.$i.20 -> 192.168.$i.1"

    ping \
        -I "192.168.$i.20" \
        -c 2 \
        -W 1 \
        "192.168.$i.1"
done
```

For example, when `i=3`, the command becomes:

```bash
ping -I 192.168.3.20 -c 2 -W 1 192.168.3.1
```

The options mean:

```text
-I 192.168.3.20   Use this local source IP
-c 2              Send two ping packets
-W 1              Wait up to one second for each reply
192.168.3.1       Destination router IP
```

---

## Step 7: Test SSH Access

If the LANEX Tester uses SSH to run commands on the router, test every router IP:

```bash
for i in {1..8}; do
    echo "Testing SSH to 192.168.$i.1"

    ssh \
        -o ConnectTimeout=3 \
        "ubnt@192.168.$i.1" \
        "echo Connected to 192.168.$i.1"
done
```

Replace `ubnt` with the correct router username.

---

## Step 8: Allow iperf3 Through Windows Firewall

Open PowerShell as Administrator and allow TCP ports `5201` through `5208`:

```powershell
New-NetFirewallRule `
    -DisplayName "LANEX Tester iperf3 TCP" `
    -Direction Inbound `
    -Protocol TCP `
    -LocalPort 5201-5208 `
    -Action Allow `
    -Profile Any
```

If UDP testing is also required:

```powershell
New-NetFirewallRule `
    -DisplayName "LANEX Tester iperf3 UDP" `
    -Direction Inbound `
    -Protocol UDP `
    -LocalPort 5201-5208 `
    -Action Allow `
    -Profile Any
```

---

## Step 9: Start iperf3 Servers Manually

Start eight `iperf3` servers inside WSL:

```bash
for i in {1..8}; do
    port=$((5200 + i))

    iperf3 \
        -s \
        -1 \
        -p "$port" \
        > "/tmp/iperf-server-$i.log" 2>&1 &
done
```

Check that they are listening:

```bash
ss -lntp | grep iperf3
```

You should see ports:

```text
5201
5202
5203
5204
5205
5206
5207
5208
```

---

## Step 10: Test from the Router

The router should connect to the matching WSL IP and port.

Examples:

```bash
iperf3 -c 192.168.1.20 -p 5201
iperf3 -c 192.168.2.20 -p 5202
iperf3 -c 192.168.3.20 -p 5203
```

The complete mapping is:

```text
Router 192.168.1.1 -> WSL 192.168.1.20:5201
Router 192.168.2.1 -> WSL 192.168.2.20:5202
Router 192.168.3.1 -> WSL 192.168.3.20:5203
Router 192.168.4.1 -> WSL 192.168.4.20:5204
Router 192.168.5.1 -> WSL 192.168.5.20:5205
Router 192.168.6.1 -> WSL 192.168.6.20:5206
Router 192.168.7.1 -> WSL 192.168.7.20:5207
Router 192.168.8.1 -> WSL 192.168.8.20:5208
```

---

## Step 11: Configure LANEX Tester

The server IP configuration should contain:

```text
192.168.1.20
192.168.2.20
192.168.3.20
192.168.4.20
192.168.5.20
192.168.6.20
192.168.7.20
192.168.8.20
```

The router/client IP configuration should contain:

```text
192.168.1.1
192.168.2.1
192.168.3.1
192.168.4.1
192.168.5.1
192.168.6.1
192.168.7.1
192.168.8.1
```

---

## Optional: Create a Reusable Setup Script

Create the script:

```bash
nano ~/setup-lanex-network.sh
```

Add:

```bash
#!/usr/bin/env bash

set -euo pipefail

INTERFACE="${1:-eth1}"

for i in {1..8}; do
    LOCAL_IP="192.168.${i}.20"
    NETWORK="192.168.${i}.0/24"

    if ! ip -4 addr show dev "$INTERFACE" | grep -qw "$LOCAL_IP"; then
        sudo ip addr add "${LOCAL_IP}/24" dev "$INTERFACE"
    fi

    sudo ip route replace \
        "$NETWORK" \
        dev "$INTERFACE" \
        src "$LOCAL_IP"
done

echo
echo "Configured routes:"

for i in {1..8}; do
    ip route get "192.168.${i}.1"
done
```

Make it executable:

```bash
chmod +x ~/setup-lanex-network.sh
```

Run it:

```bash
~/setup-lanex-network.sh
```

Or explicitly specify the interface:

```bash
~/setup-lanex-network.sh eth1
```

WSL network settings may be reset after `wsl --shutdown` or a Windows restart, so this script may need to be run again after WSL starts.

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

If SSH reports `Bad configuration option: pubkeyacceptedalgorithms`, your WSL OpenSSH client uses the older option name. Replace `PubkeyAcceptedAlgorithms` with `PubkeyAcceptedKeyTypes`:

```sshconfig
Host 192.168.*.1
    User ubnt
    PubkeyAcceptedKeyTypes +ssh-rsa
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
