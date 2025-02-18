# AOA Tone Fly On Speed

This plugin plays audio tones based on the Angle of Attack (AOA) and Fly On Speed 

## XPlane SDK

This plugin requires the XPlane SDK to be installed. You can download it from the [XPlane SDK](https://developer.x-plane.com/sdk/).

Currently using: X-Plane SDK 4.1.1 for Windows, Linux and Mac ZIP (X-Plane 12.1.0 & newer)


## Build

You can adjust the frequencies and durations by modifying the values in the initializeAudio() function. The tones will play continuously as long as the AOA is in their respective ranges, and will switch immediately when the AOA changes ranges.
Remember to install OpenAL development libraries on your system:
On Windows: Install OpenAL SDK
On Linux: sudo apt-get install libopenal-dev
On macOS: OpenAL is included in the system

## Install

1. Download the plugin from the Releases page.
2. Unzip the file and copy the `AOA-Tone-FlyOnSpeed.xml` file to your `Aircraft` folder.
