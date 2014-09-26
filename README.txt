ClearKey Gecko Media Plugin

Demonstrates how to build a Gecko Media Plugin to support playback of Encrypted
Media Extensions content in Firefox on Windows.

This GMP uses Windows Media Foundation to support H.264 and AAC decoding. This
may not work once the restrictions imposed by Gecko's Sandbox are tightened.

To build, you need the gmp-api headers, either from:
https://github.com/mozilla/gmp-api

Or from the content/media/gmp/gmp-api directory from mozilla-central
Mercurial repository:
https://hg.mozilla.org/mozilla-central/

To use the ClearKey GMP, build it, then run Firefox with the MOZ_GMP_PATH set
to the path of the "gmp-clearkey/devel" directory; the directory containing the
"clearkey.info" file.

Without this, Firefox can't find the plugin DLL to run.

For more details about Gecko Media Plugins:
https://wiki.mozilla.org/GeckoMediaPlugins
