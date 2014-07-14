ClearKey Gecko Media Plugin

Demonstrates how to build a Gecko Media Plugin to support playback of Encrypted
Media Extensions content in Firefox on Windows.

This GMP uses Windows Media Foundation to support H.264 and AAC decoding. This
may not work once the restrictions imposed by Gecko's Sandbox are tightened.

To use the ClearKey GMP, you must run Firefox with the MOZ_GMP_PATH set to the
path to the "gmp-clearkey" directory in the project, that is, the directory
containing the "clearkey.info" file.

Without this, Firefox can't find the plugin DLL to run.

For more details about Gecko Media Plugins:
https://wiki.mozilla.org/GeckoMediaPlugins
