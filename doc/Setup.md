Setup Dev Environment
====

### PlatformIO and related

This project uses [PlatformIO Core](https://platformio.org/install/cli) for development. The [setup process](https://docs.platformio.org/en/latest/installation.html) (on a Mac) is straightforward:

```bash
pip install -U --user platformio
```

Make sure you do already have the Python 2.7 bin dir (`$HOME/Library/Python/2.7/bin`) in `$PATH`.

Development and Upload
----

CLion IDE is being used for development. The device firmware is in the `firmware/` folder. Chdir to `firmware` and then:

```bash
platformio update

# if using CLion
platformio init --ide clion --board mkrwifi1010 # this will create the excluded file CMakeListsPrivate.txt
```

To e.g. upload a new firmware via USB, do:

```bash
platformio run --target upload --upload-port /dev/cu.usbmodem14701
```

Clion IDE
--

Install [CLion](https://www.jetbrains.com/clion/) (optional).

Note: the Clion IDE is useful for development, but not a hard requirement, a regular Editor, e.g. atom is also fine.
