# FlipTracker

Fliptracker is a tool for tracking flips in RuneScape. It was made for the flipchat1 community, more information at [their website](http://flipchat1rs.com/).

## Building
Fliptracker1 currently only supports linux, if someone feels motivated to port it feel free to do so. :smirk:

```sh
mkdir build
cd build
cmake ..
cmake --build . -- -j `nproc --all`
```

## Running
It is possible to run Fliptracker1 by simply running the generated binary in the `build` folder of the previous step.
```sh
./FlipTracker
```

## Install/desktop files
To get Fliptracker1 to show up in the startup menu, copy the `.desktop` file and the generated binary to `~/.local/share/applications`, and copy the image to `~/.local/share/icons`
