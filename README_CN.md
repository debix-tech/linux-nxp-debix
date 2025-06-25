* [English](./README.md) | [简体中文](./README_CN.md)

NXP系统SDK下载

- Ubuntu 22.04 :
  https://github.com/nxp-imx/meta-nxp-desktop
  
- Yocto-Linux 6.1.22_2.0.0
  https://www.nxp.com/design/software/embedded-software/i-mx-software/embedded-linux-for-i-mx-applications-processors:IMXLINUX?

  

> 下面是关于如何编译kernel和更新kernel 到SD card



# 1.下载debix-kernel源码

a.使用下面的命令clone提供的kernel源码：

```shell
git clone https://github.com/debix-tech/linux-nxp-debix.git
```

进入`linux-nxp-debix`，切换对应的分支，以**lf_6.1.22-debix_model_ab分支**为例：

b.查看所有的分支:

```shell
git branch -a
```

```shell
ljm@polyhex:~/workstation/Github/linux-nxp-debix$ git branch -a
* debix
  remotes/origin/Debix_SOM_A-L6.6.36
  remotes/origin/EMB-IMX8MP-07-L6.6.36
  remotes/origin/HEAD -> origin/debix
  remotes/origin/ModelC_linux-6.1.36_2.1.0
  remotes/origin/ModelC_linux_6.1.22
  remotes/origin/SOM_A_IO_BOARD
  remotes/origin/SOM_A_IO_BOARD-L5.15.71
  remotes/origin/SOM_A_IO_BOARD-L6.1.22
  remotes/origin/debix
  remotes/origin/development
  remotes/origin/lf_5.10.72-debix_model_a
  remotes/origin/lf_5.10.9-debix_model_a
  remotes/origin/lf_5.15.71-debix_model_ab
  remotes/origin/lf__6.1.22-debix_model_ab
  remotes/origin/lf_6.6.36-debix_mo


```

c.切换分支：`git checkout lf_6.1.22-debix_model_ab `

```shell
ljm@polyhex:~/workstation/Github/linux-nxp-debix$ git checkout lf_6.1.22-debix_model_ab
Updating files: 100% (55080/55080), done.
Branch 'lf_6.1.22-debix_model_ab' set up to track remote branch 'lf_6.1.22-debix_model_ab' from 'origin'.
Switched to a new branch 'lf_6.1.22-debix_model_ab'

```

# 2.安装交叉编译器

a.安装依赖

To build the sources for cross-compilation, make sure you have the dependencies needed on your machine by executing：

```shell
sudo apt install git bc bison flex libssl-dev make libc6-dev libncurses5-dev
```

b.下载交叉编译器

```shell
sudo mkdir /opt/toolchain
cd /opt/toolchain
sudo wget https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-a/9.2-2019.12/binrel/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu.tar.xz
sudo tar xpf gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu.tar.xz
export PATH=$PATH:/opt/toolchain/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/bin
```

注意🔔这个export是只适用于当前的终端，重新打开终端时需要再export

```shell
export PATH=$PATH:/opt/toolchain/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/bin
```

# 3.交叉编译kernel

在刚刚下载的kernel源码路径下`linux-nxp-debix`执行：

a.编译.config

```shell
export PATH=$PATH:/opt/toolchain/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/bin
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu-  imx_v8_defconfig
```

b.Compile kernel

```shell
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu-  
```

(推荐)如果觉得编译慢的话，可以通过参数-j 32来加快速度,需要根据自己电脑配置来，命令如下：

```SHELL
make -j 32 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu-  
```

**至此kernel编译完成！**

**至此kernel编译完成！**

**至此kernel编译完成！**

# 4.编译module和安装image到SD card

Having built the kernel, you need to copy it onto your Debix and install the modules; this is best done directly using an SD card reader. Prepare an SD card with Debix OS installed beforehand.

💡 **提示**以下操作都是在kernel源码路径下操作--------linux-nxp-debix

a.查看SD card

First, use `lsblk` before and after plugging in your SD card to identify it. You should end up with something a lot like this：

```shell
sdb      8:16   1  29.7G  0 disk 
├─sdb1   8:17   1   500M  0 part 
└─sdb2   8:18   1  29.1G  0 part 
```

with `sdb1` being the FAT (boot) partition, and `sdb2` being the ext4 filesystem (root) partition.

注意🔔这里面枚举出来的sdb1和sdb2每个电脑可能不一样

b.挂载

```shell
mkdir mnt
mkdir mnt/fat32
mkdir mnt/ext4
sudo mount /dev/sdb1 mnt/fat32
sudo mount /dev/sdb2 mnt/ext4
```



c.安装kernel module到SD card

```shell
export PATH=$PATH:/opt/toolchain/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/bin 
sudo env PATH=$PATH make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu-  INSTALL_MOD_PATH=mnt/ext4 INSTALL_MOD_STRIP=1 modules_install
sudo umount mnt/ext4
```

d.拷贝kernel和dts到SD card

备份一份原来的image，避免出现无法恢复

```shell
sudo cp mnt/fat32/Image mnt/fat32/Image-backup.img
```

拷贝新的image和设备树到SD card

```shell
sudo cp arch/arm64/boot/Image mnt/fat32/Image
sudo cp arch/arm64/boot/dts/freescale/*.dtb mnt/fat32/. 
sudo umount mnt/fat32
```

**Finally, plug the card into Debix and boot it!**

**Finally, plug the card into Debix and boot it!**

**Finally, plug the card into Debix and boot it!**



# 5.配置kernel菜单

通常driver开发需要配置kernel的config，下面是参考:

安装依赖：

```shell
sudo apt install libncurses5-dev
```

a.进入菜单，选择需要的配置

如果没有执行过上面的`export`命令，需要执行来导出环境:

```shell
export PATH=$PATH:/opt/toolchain/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/bin
make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- menuconfig
```

之后就会弹出kernel的菜单，可以在上面选择通过键盘的上下左右按键来移动，通过回车按键进入，空格键选择编译(*,M)或者不编译

b.保存修改

Once you’ve done making the changes you want, press `Escape` until you’re prompted to save your new configuration. By default, this will save to the `.config` file. You can save and load configurations by copying this file around.