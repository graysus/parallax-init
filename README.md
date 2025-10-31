## Parallax-init ([v0.04](./PXVERSION))
This init system aims to have the user-friendliness and modernity of Systemd, but with the lightness and portability of traditional init systems like OpenRC.<br>
Keep in mind that this is an **ALPHA RELEASE**, so you may encounter strange issues, especially on more configurations. <br>
Also keep in mind that this init system is missing many features, and is tested only on Artix Linux. **Additionally, this init system only supports Linux currently, and LIBCs other than Glibc are not tested.** <br>
This software is very young however, so if you find any issues with these environments, I'd appreciate if you submitted a pull request!

## Building and installing
To build, just run:
```sh
./configure --clean --all
```
Then, to install:
```sh
make install
### or ###
make DESTDIR="$PKGDIR" install
```
Add this line to the bottom of `/etc/pam.d/system-login`:
```
-session    optional    pxpam.so
```
Service files will be provided in a separate repository at a later date.