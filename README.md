# LAN-EX Head/Field Tester #

Software to automatically test the correct data throughput between up to 8 LAN-EXs pairs at the same time
using Linux platform and EdgeRouter12.

## Build ##

* Clone the repository to a Linux platform
* Install the required dependencies: (libncurses)
	* (ubuntu): `apt install libncurses5 libncurses5-dev`
* run `make build`

## Run ##
### Configuration ###
* Setup 8 aliases on main ethernet interface on 8 separate networks

    ### For temporarily setting up the aliases or setting them up with a script ###
	* (ubuntu): `ifconfig <interfaceName>:1 192.168.1.20 up` 
		* i.e. `ifconfig eth3:1 192.168.1.20 up`
	* (ubuntu): `ifconfig <interfaceName>:2 192.168.2.20 up` 
	* etc...
	
	### For setting up the aliases permanently ### 
	* (ubuntu 18.1+): open the file `/etc/netplan/01-netcfg.yaml` with your favourite editor
	* (ubuntu 18.1+): append the following configuration under the renderer field (change the name of the interface with the name of the interface you wish to use during the test)
	```
	ethernets:
     <interfaceName>:
      addresses: [  ]
      gateway4: 192.168.1.1
	```
	* (ubuntu 18.1+): add the addresses for each alias in the addresses field. 
		* i.e. `addresses: [ 192.168.1.20/24, 192.168.2.20/24, ...]`
	* (ubuntu 18.1+): `netplan apply` 


* Setup 8 interfaces on the EdgeRouter each on a separate network matching a corresponding network of an alias
	* (example for eth0)
	* Log-in into the EdgeRouter web dashboard
	* Click on the actions dropdown for eth0 -> Config
	* Address: Manually define IP addresses
	* On the address text-box insert: 192.168.1.1
	* Click save
	* Repeat for eth1 to eth7 and change network for the address for each interface
	
* Create an ssh-key pair and copy it in the edge router
	* ssh-keygen -t rsa -b 4096 (leave the passphrase empty, the software cannot enter passphrases)
	* copy the public key into the EdgeRouter (`scp <pathOfPublicKey> ubnt@<EdgeRouterIp>:/ubnt/home/.ssh/`)
	* log into the router CLI
	* `configure`
	* `loadkey ubnt <pathOfCopiedPublicKey>`
	* `save`
	* `exit`
* Allow RSA/SHA-1 keys for the EdgeRouter in the SSH client configuration
	* IMPORTANT: newer OpenSSH clients (Ubuntu 22.04+) disable RSA/SHA-1 signatures by default, but the EdgeRouter firmware only supports that flavour. Without this step key authentication silently fails, ssh falls back to an invisible password prompt and the software hangs forever on the test screen (TX/RX stuck on `tmp`, countdown going negative).
	* Add the following to `~/.ssh/config` (create the file if needed) of the user that runs the software (adjust the Host pattern if your EdgeRouter IPs differ):
	```
	Host 192.168.*.1
	    User ubnt
	    PubkeyAcceptedAlgorithms +ssh-rsa
	    HostKeyAlgorithms +ssh-rsa
	```
	* `chmod 600 ~/.ssh/config`
* Verify passwordless SSH works for ALL EdgeRouter interfaces (the software cannot answer password or host-key prompts)
	* run: `for i in 1 2 3 4 5 6 7 8; do ssh -o BatchMode=yes ubnt@192.168.$i.1 echo "OK $i"; done`
	* every line must print `OK <n>`. If an IP fails with a host key verification error, connect to it once interactively (`ssh ubnt@192.168.<n>.1`) and answer `yes` to the fingerprint prompt, then verify again. If it fails with `Permission denied`, re-check the key setup and the `~/.ssh/config` step above.
* Edit `config\targetBandwidth.conf` with the target bandwidth and duration for the test
* Edit `config\serverIps.conf` with the all the aliases of the PC network interface
* Edit `config\clientIps.conf` with all the IPs of the interfaces of the EdgeRouter
* Create the following folders `reports`, `reports/eng`

### Start the software ###
* run `make run`

## Report Generation ##
* The software after each tests will create 3 files
	* `reports\allReports.txt`: contains a continuous list with all the test successful test results
	* `reports\<ConfigurationName>_<SerialNumber>.txt`: contains the result of a single successful test 
	* `reports\eng\eng_<ConfigurationName>_<SerialNumber>.txt`: contains all the logs of the iperf3 instances ran for a single test

