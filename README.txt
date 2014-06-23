ClearKey Gecko Media Plugin

Demonstrates how to build a Gecko Media Plugin to support playback of Encrypted
Media Extensions content in Firefox on Windows.

This GMP uses Windows Media Foundation to support H.264 and AAC decoding. This
may not work once Gecko's Sandbox is turned on for GMPs.

To use the ClearKey GMP, you must first add a registry key:
  
  Computer\HKEY_CURRENT_USER\Software\MozillaPlugins\ClearKey

with child string value called "Path" whose value is the path to the
"gmp-clearkey" directory in the project, that is, the directory containing
the "clearkey.info" file.

Without this, Firefox can't find the plugin DLL to run.

For more details about Gecko Media Plugins:
https://wiki.mozilla.org/GeckoMediaPlugins
