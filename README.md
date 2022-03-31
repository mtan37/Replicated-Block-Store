# Replicated-Block-Store
Project 3 for the distributed system class

# Compiling

  ```
  > mkdir build
  > cd build
  > cmake ..
  > make
  ```
> If you installed cmake locally, please add -DCMAKE_PREFIX_PATH=<installation path> to your cmake flag
# Run the project

  ```
  > ./server <current server IP> <alt server IP>
  ```
  
 # Local testing
 Open two terminal and one of each command in each terminal
  ```
  > ./server localhost localhost
  ```
  ```
  > ./server localhost localhost -alt
  ```


# FUSE/loopback

First start the servers. Then start the fuse process:

  ```
  > ./fuse_backing <fuse_mount_point> -d
  ```

Note that the mount point should be a regular file, not a directory. Next create the loop device:

  ```
  > sudo losetup -fP <fuse_mount_point>
  ```

At this point the loop device will be at `\dev\loop?`. You can find the exact device by looking at the output of

  ```
  > losetup -a
  ```

At this point you can treat the device as you would an other block device. For example you could create a new partition table (use parted as fdisk seems to have problems), format with a filesystem, and mount. For example, if the device is `\dev\loop1`, you might do

  ```
  > sudo parted -s /dev/loop1 mklabel gpt
  > sudo parted -s /dev/loop1 mkpart primary 1MiB 100%
  > sudo mkfs.ext4 /dev/loop1p1
  > sudo mount /dev/loop1p1 <fs_mount_point>
  > sudo chmod 0777 <fs_mount_point>
  ```

When you are done, detach the loop device with

  ```
  > sudo losetup -d <loop_dev>
  ```

Afterwhich you can unmount the FUSE.
