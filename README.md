[English](./README.md) | [简体中文](./README_CN.md)

### Host Setup
The recommended minimum Ubuntu version is 22.04 or later.
Essential Yocto Project host packages are:
```
sudo apt-get install build-essential chrpath cpio debianutils diffstat file gawk gcc git iputils-ping libacl1 liblz4-tool locales python3 python3-git python3-jinja2 python3-pexpect python3-pip python3-subunit socat texinfo unzip wget xz-utils zstd efitools
```

### Download the compiler
```shell
## L6.12.49 use gcc 14.3 
debix@polyhex:$ wget https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/14.3.rel1/binrel/arm-gnu-toolchain-14.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz

debix@polyhex:$ tar xpf arm-gnu-toolchain-14.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz -C /opt/toolchain
```

### Configure the compilation environment
> export PATH=$PATH:/opt/toolchain/arm-gnu-toolchain-14.2.rel1-x86_64-aarch64-none-linux-gnu/bin
> export ARCH=arm64
> export CROSS_COMPILE=aarch64-none-linux-gnu-
  
### build dtb Image
> make imx_v8_defconfig
> make -j32
 
compiled and generated
arch/arm64/boot/dts/freescale/imx93-emb-13-a1.dtb
arch/arm64/boot/dts/freescale/imx91-emb-13-a1.dtb
arch/arm64/boot/Image

cp dtb and Image to Debix /boot/ complete kernel update

 
