# DarkPlayer
A neumorphic music player for Windows 10 / 11.

Based on concepts by [Filip Legierski](https://dribbble.com/kedavra).

<p align="center">
  <img src="screenshot.png" width=38% alt="Screenshot"/>
</p>

## Behavior:
DarkPlayer recurses through your music folder on launch and finds any albums that are there. You can then play them from the hamburger menu.

## Building:
1. Open solution in Visual Studio 2022
2. Build

## Q & A:
**Q: Why does the code look like cancer?**

**A:** Because it is.

**Q: Why not just use the default windows media player?**

**A:** I didn't really know how good the Win11 media player was when I started making this. I was trying to replace the garish light-themed legacy Win10 media player that was burning my eyes out. But hey at least mine has a visualizer, which both media players lack.

**Q: No albums are showing up!**

**A:** Albums are found by tracks sharing the same album string in their metadata. You can edit track metadata and make albums from loose audio files with [Mp3tag](https://www.mp3tag.de/en/).