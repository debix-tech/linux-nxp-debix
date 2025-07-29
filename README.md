* [English](./README.md) | [简体中文](./README_CN.md)

NXP System SDK Download  

- Yocto-Linux 6.12.3_1.0.0  
  https://www.nxp.com/design/software/embedded-software/i-mx-software/embedded-linux-for-i-mx-applications-processors:IMXLINUX?  



> The following is about how to compile the kernel and update it to the SD card.  



# 1. Download debix-kernel Source Code  

a. Use the following command to clone the provided kernel source code:  

```shell
git clone https://github.com/debix-tech/linux-nxp-debix.git
```

Enter the `linux-nxp-debix` directory and switch to the corresponding branch. Take the **lf_6.12.3-debix_model_ab branch** as an example:  

b. View all branches:  

```shell
git branch -a
```

```shell
ljm@polyhex:~/workstation/Github/linux-nxp-debix$ git branch -a
  Model_AB-L6.6.36
  debix
* lf_6.12.3-debix_model_ab
  lf_6.6.36-debix_model_ab
  remotes/origin/Debix_SOM_A-L6.6.36
  remotes/origin/EMB-IMX8MP-07-L6.6.36
  remotes/origin/HEAD -> origin/debix
  remotes/origin/ModelC_linux-6.1.36_2.1.0
  remotes/origin/ModelC_linux_6.1.22
  remotes/origin/Model_AB-L5.15.71
  remotes/origin/Model_AB-L6.1.22
  remotes/origin/Model_AB-L6.6.36
  remotes/origin/SOM_A_IO_BOARD
  remotes/origin/SOM_A_IO_BOARD-L5.15.71
  remotes/origin/SOM_A_IO_BOARD-L6.1.22
  remotes/origin/debix
  remotes/origin/debix-L5.10.72
  remotes/origin/debix-L5.10.9
  remotes/origin/development
  remotes/origin/lf_6.6.36-debix_model_ab
  remotes/origin/yocto-L5.10.72


```

c. Switch branches: `git checkout lf_6.12.3-debix_model_ab`  

```shell
ljm@polyhex:~/workstation/Github/linux-nxp-debix$ git checkout lf_6.12.3-debix_model_ab
Branch 'lf_6.12.3-debix_model_ab' set up to track remote branch 'lf_6.12.3-debix_model_ab' from 'origin'.
Switched to a new branch 'lf_6.12.3-debix_model_ab'

```

# 2. Install Cross-Compiler  

a. Install dependencies  

To build the sources for cross-compilation, make sure you have the dependencies needed on your machine by executing:  

```shell
sudo apt install git bc bison flex libssl-dev make libc6-dev libncurses5-dev
```

b. Download the cross-compiler 

```shell
sudo mkdir /opt/toolchain
cd /opt/toolchain
sudo wget https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-a/9.2-2019.12/binrel/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu.tar.xz
sudo tar xpf gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu.tar.xz
export PATH=$PATH:/opt/toolchain/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/bin
```

Note🔔This `export` is only valid for the current terminal. You need to re-export it when opening a new terminal:  

```shell
export PATH=$PATH:/opt/toolchain/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/bin
```

# 3. Cross-Compile the Kernel  

Execute the following in the `linux-nxp-debix` directory where the kernel source code was just downloaded:  

a. Compile `.config`  

```shell
export PATH=$PATH:/opt/toolchain/arm-gnu-toolchain-14.2.rel1-x86_64-aarch64-none-linux-gnu/bin
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu-  imx_v8_defconfig
```

b. Compile the kernel 

```shell
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu-  
```

(Recommended) If compilation is slow, you can speed it up with the `-j 32` parameter (adjust based on your computer configuration):  

```shell
make -j 32 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu-  
```

**The kernel compilation is now complete!**  

**The kernel compilation is now complete!**  

**The kernel compilation is now complete!**  

# 4. Compile Modules and Install the Image to the SD Card

Having built the kernel, you need to copy it onto your Debix and install the modules; this is best done directly using an SD card reader. Prepare an SD card with Debix OS installed beforehand.  

💡 **Tip** The following operations are all performed in the kernel source code directory — `linux-nxp-debix`.  

a. View the SD card  

First, use `lsblk` before and after plugging in your SD card to identify it. You should end up with something a lot like this： 

```shell
sdb      8:16   1  29.7G  0 disk 
├─sdb1   8:17   1   500M  0 part 
└─sdb2   8:18   1  29.1G  0 part 
```

with `sdb1` being the FAT (boot) partition, and `sdb2` being the ext4 filesystem (root) partition. 

Note🔔The enumerated `sdb1` and `sdb2` may differ on each computer.  

b. Mount the partitions  

```shell
mkdir mnt
mkdir mnt/fat32
mkdir mnt/ext4
sudo mount /dev/sdb1 mnt/fat32
sudo mount /dev/sdb2 mnt/ext4
```



c. Install kernel modules to the SD card 

```shell
export PATH=$PATH:/opt/toolchain/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/bin 
sudo env PATH=$PATH make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu-  INSTALL_MOD_PATH=mnt/ext4 INSTALL_MOD_STRIP=1 modules_install
sudo umount mnt/ext4
```

d. Copy the kernel and dts to the SD card  

Back up the original image to avoid unrecoverable issues:  

```shell
sudo cp mnt/fat32/Image mnt/fat32/Image-backup.img
```

Copy the new image and device tree to the SD card:  

```shell
sudo cp arch/arm64/boot/Image mnt/fat32/Image
sudo cp arch/arm64/boot/dts/freescale/*.dtb mnt/fat32/. 
sudo umount mnt/fat32
```

**Finally, plug the card into Debix and boot it!**  

**Finally, plug the card into Debix and boot it!** 

**Finally, plug the card into Debix and boot it!** 



# 5. Configure the Kernel Menu  

Generally, driver development requires configuring the kernel's config. The following is a reference:  

Install dependencies:  

```shell
sudo apt install libncurses5-dev
```

a. Enter the menu and select the required configurations 

If you haven’t executed the above `export` command, run it to set the environment: 

```shell
export PATH=$PATH:/opt/toolchain/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/bin
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- menuconfig
```

Then the kernel menu will appear. You can navigate using the arrow keys (up/down/left/right) on your keyboard, press Enter to select an entry, and use the spacebar to choose compilation options (*,M) or disable compilation 

b. Save modifications  

Once you’ve done making the changes you want, press `Escape` until you’re prompted to save your new configuration. By default, this will save to the `.config` file. You can save and load configurations by copying this file around.



