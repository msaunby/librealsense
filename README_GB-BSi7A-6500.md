# GB-BSi7A-6500 Setup Notes for librealsense (R200)

These notes are for bringing up this repository on a Gigabyte GB-BSi7A-6500 with a dual-boot Linux setup:

- Ubuntu 16.04 LTS: known-good target for legacy R200 support in this repo
- Ubuntu 22.04 LTS: longer-life target for porting and maintenance

## 1) Recommended Dual-Boot Layout (120GB SSD)

For a 120GB SSD, use this concrete split:

- EFI System Partition (shared): 512 MB (reuse existing)
- Ubuntu 16.04 root (/): 25 GB (ext4)
- Ubuntu 22.04 root (/): 35 GB (ext4)
- Shared data partition: remainder, about 58-59 GB (ext4)
- Swap: use swapfile on each OS (default installer behavior)

Notes:

- A 120GB SSD typically appears as about 111 GiB usable in Linux tools
- The shared partition should hold user data, source trees, logs, and test captures
- Label the shared partition as rs-data during install for easy mounting

Why this split:

- 16.04 stays isolated as a stable reference environment for camera bring-up
- 22.04 is where to test modern kernel/toolchain porting
- Shared data partition avoids duplicated source trees and logs

### Shared Partition Mount Strategy

On both OS installs, mount the shared partition at /data and keep projects in /data/projects.

After install, verify the shared partition label and UUID:

```bash
lsblk -f
sudo blkid
```

Add an fstab entry on both OSes (same UUID value):

```bash
UUID=<shared-partition-uuid> /data ext4 defaults,nofail 0 2
```

Then create standard folders once:

```bash
sudo mkdir -p /data/projects /data/logs
sudo chown -R $USER:$USER /data
```

Optional convenience link from your home directory:

```bash
ln -s /data/projects ~/projects
```

### Installer Click Path (Exact)

Use this install order:

1. Install Ubuntu 16.04 first
2. Install Ubuntu 22.04 second

Reason: newer GRUB from 22.04 usually detects and manages the older 16.04 install better.

#### A) Install Ubuntu 16.04 (first pass)

1. Boot the 16.04 USB installer in UEFI mode
2. Choose Install Ubuntu
3. At Installation type, choose Something else
4. In the partition table, create or assign:
  - EFI System Partition, 512 MB, FAT32, mount point /boot/efi, do not format if it already exists
  - ext4, 25 GB, mount point /
  - ext4, 35 GB, leave unassigned for now (this will become 22.04 root later)
  - ext4, remaining space, leave unassigned for now (this will become shared data)
5. Device for boot loader installation: select the whole disk (example: /dev/sda), not a single partition
6. Continue install and reboot into 16.04

After first boot into 16.04:

1. Create and label the shared data partition from the unassigned space as ext4 label rs-data
2. Leave the future 22.04 root partition ready as ext4 without extra data

#### B) Install Ubuntu 22.04 (second pass)

1. Boot the 22.04 USB installer in UEFI mode
2. Choose Install Ubuntu
3. At Installation type, choose Something else
4. Assign partitions carefully:
  - Existing EFI System Partition: mount point /boot/efi, do not format
  - ext4 partition reserved for 22.04: mount point /, format this partition
  - Shared ext4 partition (label rs-data): no mount point in installer is fine, or set /data and do not format if you already populated it
  - 16.04 root partition: do not use, do not format
5. Device for boot loader installation: select the whole disk (example: /dev/sda)
6. Finish install and reboot

After first boot into 22.04:

1. Open a terminal and run:

```bash
sudo update-grub
```

2. Confirm both menu entries appear:
  - Ubuntu 22.04
  - Ubuntu 16.04 (or Advanced options for Ubuntu with the older kernel series)

#### C) Make Shared Data Available in Both OSes

Run on each OS:

```bash
lsblk -f
sudo blkid
```

Add the same UUID entry to /etc/fstab on both OS installs:

```bash
UUID=<shared-partition-uuid> /data ext4 defaults,nofail 0 2
```

Then:

```bash
sudo mkdir -p /data/projects /data/logs
sudo chown -R $USER:$USER /data
ln -s /data/projects ~/projects
```

Validation:

1. Boot into 16.04 and create a test file under /data/projects
2. Reboot into 22.04 and verify the same file is visible
3. Remove the test file

## 2) BIOS / Firmware Prep

Before OS installation:

1. Update BIOS to latest available for GB-BSi7A-6500
2. Enable UEFI boot mode
3. Disable Secure Boot (recommended for easier custom kernel module flow)
4. Keep xHCI/USB 3.0 enabled
5. Prefer direct USB 3.0 port connections for R200 (avoid hubs during bring-up)

## 3) Ubuntu 16.04 Baseline (Known-Good Path)

This repo's Linux docs target Ubuntu 14.04/16.04 era kernels and scripts.

### Packages

Run on 16.04:

```bash
sudo apt-get update
sudo apt-get install -y git build-essential cmake pkg-config \
  libusb-1.0-0-dev libglfw3-dev
```

Optional (QtCreator flow from repo docs):

```bash
sudo apt-get install -y qtcreator
sudo scripts/install_qt.sh
```

### Repo Setup

```bash
cd ~/projects
git clone <your-fork-or-origin-url> librealsense
cd librealsense
```

### Udev Rules

```bash
sudo cp config/99-realsense-libusb.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### Kernel UVC Patch (critical for legacy R200 color formats)

For Ubuntu 16.04 / kernel 4.4 path in this repo:

```bash
./scripts/patch-uvcvideo-16.04.simple.sh
sudo modprobe uvcvideo
sudo dmesg | tail -n 50
```

### Build

```bash
mkdir -p build && cd build
cmake .. -DBUILD_EXAMPLES:BOOL=true
make -j"$(nproc)"
```

Quick sanity test:

```bash
./bin/cpp-enumerate-devices
```

## 4) Ubuntu 22.04 Porting Track (Long-Life Path)

Expect additional work here. The 16.04 patch script will not directly map to newer kernels.

### Packages

Run on 22.04:

```bash
sudo apt update
sudo apt install -y git build-essential cmake pkg-config \
  libusb-1.0-0-dev libglfw3-dev libudev-dev
```

### First Build Attempt

```bash
cd ~/projects/librealsense
mkdir -p build-22.04 && cd build-22.04
cmake .. -DBUILD_EXAMPLES:BOOL=true
make -j"$(nproc)"
```

### Porting Notes

- Start with depth/infrared streaming first
- Treat color stream enablement as a separate milestone
- The legacy UVC format patch likely needs manual adaptation for modern kernel source
- Keep 16.04 results as your known-good comparison baseline

## 5) Practical Validation Sequence (Both OSes)

1. Plug one R200 directly into a rear USB 3.0 port
2. Run enumeration example and capture logs
3. Verify depth + infrared first
4. Test color modes after baseline streams are stable
5. Repeat on second camera only after first is stable

Suggested logging commands:

```bash
uname -a
lsusb
dmesg | tail -n 120
```

## 6) Day-1 Checklist

- BIOS updated
- Dual-boot installed (16.04 + 22.04)
- Repo cloned on shared data partition or separately per OS
- 16.04 dependencies installed
- Udev rules installed
- UVC patch script run on 16.04
- 16.04 build and enumerate test passing
- 22.04 build attempt completed and issues logged

## 7) Expected Outcome

- Ubuntu 16.04 should give the fastest path to reliable R200 bring-up
- Ubuntu 22.04 is for incremental porting and longer-term maintainability
- This dual-track setup minimizes risk while preserving forward progress
