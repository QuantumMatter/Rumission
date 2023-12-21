# Zephyr Projects

## General

* T2 Topology (`west-ws/application` is the manifest repo)
* The actual FW applications live at
  * Main Board: `west-ws/applications/main`
  * Flow Meter: To be added

## Building

```bash
cd west-ws
west build -p always -b rumission_backpack_main applications/main
```

## Flashing

```bash
cd west-ws
west flash
```

## WSL Process

### West / Zephyr Setup

Make sure that the west workspace is installed on the linux filesystem, NOT the mounted windows filesystem (`/mnt/c`). If the repo has been cloned into something like `$HOME`, then your fine. Otherwise, make a symbolic link that points to the `west-ws/application` folder.

```bash
cd ~
mkdir rumission-west-ws
cd rumission-west-ws
ln -s <repo-dir>/Firmware/west-ws/application application
```

Intialize the west workspace. We'll need to manually create our west config file because the `west init` command will not respect our symbolic link. Paste the following contents into a new file at `~/rumission-west-ws/.west/config`

```
[manifest]
path = application
file = west.yml
```

Now we can finally init (update) our west workspace

```bash
cd ~/rumission-west-ws
west update
```

### USB Devices

We'll need to install `usbipd` to pass devices through to WSL. Download and install it from [Microsoft](https://learn.microsoft.com/en-us/windows/wsl/connect-usb#install-the-usbipd-win-project), and then restart your computer.

You'll also need to install some packages in the WSL distro to receive the device

```bash
sudo apt install linux-tools-generic hwdata
sudo update-alternatives --install /usr/local/bin/usbip usbip /usr/lib/linux-tools/*-generic/usbip 20
```

Open Powershell with Admin privileges. Use the new `usbipd` command to list the available devices and attach one to WSL. Also note that you may be prompted to update your WSL installation.

```ps
PS C:\Users\kopal> usbipd list
Connected:
BUSID  VID:PID    DEVICE                                                        STATE
1-2    0b05:19af  AURA LED Controller, USB Input Device                         Not shared
1-3    413c:2107  USB Input Device                                              Not shared
1-7    10c4:ea60  Silicon Labs CP210x USB to UART Bridge (COM4)                 Shared
1-9    05ac:0503  USB Input Device                                              Not shared
1-10   1b1c:0c1a  USB Input Device                                              Not shared
1-14   8087:0033  Intel(R) Wireless Bluetooth(R)                                Not shared

Persisted:
GUID                                  DEVICE

PS C:\Users\kopal> usbipd wsl attach --busid 1-7
usbipd: info: Using default WSL distribution 'Ubuntu'; specify the '--distribution' option to select a different one.
PS C:\Users\kopal>
```

Now you should see it in WSL

```bash
$ lsusb
Bus 002 Device 001: ID 1d6b:0003 Linux Foundation 3.0 root hub
Bus 001 Device 002: ID 10c4:ea60 Silicon Labs CP210x UART Bridge
Bus 001 Device 001: ID 1d6b:0002 Linux Foundation 2.0 root hub
```

Now, we can flash it through WSL

```bash
$ cd ~/rumission-west-ws
$ west build -p always -b rumission_backpack_main applications/main
$ sudo su
$ /home/davidkopala/miniconda3/envs/zephyr_311/bin/python -m west flash
$ minicom -D /dev/ttyUSB0 -c on
$ exit
```

## Debugging 

### Coredumps

```bash
$ cd ~/rumission-west-ws
$ ./zephyr/scripts/coredump/coredump_serial_log_parser.py application/coredump.log coredump.bin
$ zephyr/scripts/coredump/coredump_gdbserver.py build/zephyr/zephyr.elf coredump.bin -v
```

```bash
$ cd ~/rumission-west-ws
$ ~/zephyr-sdk-0.16.1/xtensa-espressif_esp32_zephyr-elf/bin/xtensa-espressif_esp32_zephyr-elf-gdb build/zephyr/zephyr.elf
(gdb) target remote localhost:1234
```