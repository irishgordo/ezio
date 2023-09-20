# EZIO Developer and User Guide

![build test](https://github.com/tjjh89017/ezio/actions/workflows/github_actions.yml/badge.svg)

## Introduction

EZIO is a tool for rapid server disk image cloning/deployment within local area network. We utilize BitTorrent protocol to speed up the data distribution. Also, we use `partclone` to dump used filesystem blocks, and EZIO receiver can directly write received blocks to raw disk, which greatly improves performance. 

## Motivation

EZIO is inspired by Clonezilla and BTsync (Resilio) for its idea to transfer data and massive deployment. The issue of Clonezilla is that, it is too slow in real world due to multicast feature. In real world, all clients must register to Clonezilla server before starting deployment, which cost too much time. In addition, whenever there is a client that doesn't get data or gets incorrect one and need to be re-transferred, it causes server a lot of effort. Most importantly, in most case, the clients which cannot get data correctly may be broken, it will make server to re-transfer data again and again until it reaches its re-transfer limit and quit. Due to above issues of Clonezilla, EZIO make a difference by changing transfer mechanism. EZIO implement transfer function on top of BitTorrent, and make a lot of progress on deployment speed. 

## Feature


- Faster than Clonezilla by implementing data transfer on top of the BitTorrent protocol. Clonezilla uses multicast for transfer,for which in practice are extremely slow due to limitation of multicast and clients'  status. Limitation of multicast, for example, they will cost too much time waiting all the clients to register to the server. As for Computer status, for example, when there are a small amount of computers which don't have enough disk storage or might be broken, in this case, they won't get data from server successfully and need to re-transfer data, which  cost lots of time.



- Plenty of File systems are supported: 
    (1) ext2, ext3, ext4, reiserfs, reiser4, xfs, jfs, btrfs, f2fs and nilfs2 of GNU/Linux
    (2) FAT12, FAT16, FAT32, NTFS of MS Windows
    (3) HFS+ of Mac OS
    (4) UFS of FreeBSD, NetBSD, and OpenBSD
    (5) minix of Minix
    (6) VMFS3 and VMFS5 of VMWare ESX. 
    Therefore you can clone GNU/Linux, MS windows, Intel-based Mac OS, FreeBSD, NetBSD, OpenBSD, Minix, VMWare ESX and Chrome OS/Chromium OS, no matter it's 32-bit (x86) or 64-bit (x86-64) OS. For these file systems, only used blocks in partition are saved and restored. For unsupported file system, sector-to-sector copy is done by dd in EZIO.

- Different from BTsync file level transfer, EZIO is block level transfer. Whenever a client gets wrong data, in file level transfer, it will take a lot of time re-transfer whole file. However, in block level transfer, all we need to do is to re-transfer the specific piece of data.

- Saves data in the hard disk by using partclone. 


# Installation

### Minimum System Requirements

- 64bit
- 1GB RAM

### Dependencies
- Debian 11 or above
- libtorrent-rasterbar>=2.0.8
- libboost>=1.74
- cmake>=3.16
- spdlog
- gRPC
```shell
sudo apt install build-essential cmake libboost-all-dev libtorrent-rasterbar-dev libgrpc-dev libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc libspdlog-dev
```

### Build and Install

```shell
mkdir build
cd build
cmake ../
make
sudo make install
```

We also provide a Dockerfile for the ease of installation and CI testing.
To build the image type this:

```shell
docker build . -t ezio-latest-img
```

## Usage

### Partclone

[Partclone](http://partclone.org/) provides utilities to save and restore used filesystem blocks **(and skips the unused blocks)** from/to a partition.

The newest partclone will support dump your disk to EZIO image, and generate `torrent.info` simultaneously.
```shell
sudo partclone.extfs -c -T -s /dev/sda1 -O target/ --buffer_size 16777216
```
or you want generate torrent, but don't want BT image.
```shell
sudo partclone.extfs -c -t -s /dev/sda1 -O target/ --buffer_size 16777216
```

When finishing to dump disk, you will see the file like the picture. And using `utils/partclone_create_torrent.py` to generate torrent for deploy.
![](https://i.imgur.com/8o815PL.png)

```shell
utils/partclone_create_torrent.py -c CloneZilla -p sda1 -i <some_path>/torrent.info -o sda1.torrent -t 'http://<some tracker>:6969/announce'
```

### EZIO

When you have a `sda1.torrent` you can deploy or clone your disk via Network.

#### Help

```
Allowed Options:
  -h [ --help ]          some help
  -F [ --file ]          read data from file rather than raw disk
  --listen arg           gRPC service listen address and port, default is 
```

#### Seeding

- Seeding from BT image
```shell
./ezio -F
./utils/create_proto_py.sh
./utils/add_torrent_seed.py sda1.torrent /some/path/to/sda1
```

- Seeding from Disk
```shell
./ezio
./utils/create_proto_py.sh
./utils/add_torrent_seed.py sda1.torrent /dev/sda1
```

#### Downloading

- Downloading to Disk
```shell
./ezio
./utils/create_proto_py.sh
./utils/add_torrent.py sda1.torrent /dev/sda1
```

- Proxy or save the image
```shell
./ezio -F
./utils/create_proto_py.sh
./utils/add_torrent.py sda1.torrent /some/path/to/save/sda1
```

#### Proxy

If you want to deploy over Internet or some bottleneck, you can proxy the torrent via regular BT software like [qBittorrent](https://www.qbittorrent.org/). And don't let internal peer connect outside directly.

## Easy Usage to Deploy Disk or OS via EZIO

Using CloneZilla Live (version>=testing-2.6.0-31). CloneZilla contains EZIO in its `Lite Server Mode`. It will be most easy way to deploy your disk or OS via BT.

## Design

In `raw_storage.cpp` implements a `libtorrent` [custom storage](http://libtorrent.org/reference-Custom_Storage.html#overview), to allow the receiver to write received blocks directly to raw disk.

We store the "offset" in hex into torrent, the "length" into file attribute.
so BT will know where the block is, and it can use the offset to seek in the disk

```
{
    'announce': 'http://tracker.site1.com/announce',
    'info':
    {
        'name': 'root',
        'piece length': 262144,
        'files':
        [
            {'path': ['0000000000000000'], 'length': 4096}, // store offset and length of blocks
            {'path': ['0000000000020000'], 'length': 8192},
            ...
        ],
        'pieces': 'some piece hash here'
    }
}
```

## Benchmark

Compare with CloneZilla Multicast Mode with EZIO Mode.

### Experimental environment
- Network: Cisco 3560G
- Server: Dell T1700 with Intel Xeon E3-1226, 16G ram, 1TB hard disk
- PC Client: 32 Client, same as Server
- Image: Ubuntu Linux with 50GB data in disk. Multicast Image is compressed by `pzstd`. BT Image is raw file.

### Result
Time in second

| Number of client | Time (Unicast) | Time (EZIO) | Time (Multicast) | Ratio (BT/Multicast) |
| ---: | ---: | ---: | ---: | ---: |
| 1 | 474 | 675 | 390 | 1.731 |
| 2 | 948 | 1273 | 474 | 2.686 |
| 4 | 1896 | 1331 | 638 | 2.086 |
| 8 | 3792 | 1412 | 980 | 1.441 |
| 16 | 7584 | 1005 | 1454 | 0.691 |
| 24 | 11376 | 1048 | 1992 | 0.526 |
| 32 | 15168 | 1143 | 2203 | 0.519 |

## Open Access Journal

More details about EZIO design and benchmark are in [A Novel Massive Deployment Solution Based on the Peer-to-Peer Protocol](https://www.mdpi.com/2076-3417/9/2/296).

## Limitation

- Making a torrent cost lots of time due to sha-1 hash need to be done on every single piece of data.
- EZIO will be extremely slow when the number of clients is too small.
- Due to partclone limitation, for unsupported filesystem, sector-to-sector copy is done by dd in EZIO.

## Future

- We need more speed
- gRPC to control
- Integrate in OpenStack Ironic Project, improve Mirantis' works
    - http://web.archive.org/web/20211124125644/https://www.mirantis.com/blog/cut-ironic-provisioning-time-using-torrents/
    - https://review.opendev.org/c/openstack/ironic-specs/+/311091

## Contribute

- Issue Tracker: https://github.com/tjjh89017/ezio/issues
- Source Code: https://github.com/tjjh89017/ezio

## Support 

If you are having issues, please let us know.
EZIO main developer email is located at: tjjh89017@hotmail.com

## Special Thanks

- National Center for High-performance Computing, NCHC, Taiwan
    - Provide many devices to test stability and knowledge support.

## License

The project is licensed under the GNU General Public License v2.0 license.
