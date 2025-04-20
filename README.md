# Photoshop Unlocker
Adobe Photoshop likes to interrupt your work with annoying popup messages. This counts following messages:<br>
1. Unlicenced copy warning
2. Unskipable warning when Photoshop cannot access internet
3. Forced licence accept window. You have no option to decline the licence and continue working.
<br>

This tool is created ~~to fuck Adobe~~ to give you more freedom and solve this problem. This program is realtime monitoring software to detect annouying popup windows in Photoshop. When such windows appers it instantly closes this window, unlocks Photoshop main window and makes it focused, so you can keep working without problems.

# How to use
This is not neccessary, but if you don't need cloud features and want to force the Photoshop work in the offline mode you can block Adobe hosts by adding them in your Windows **hosts** file. List of Adobe hosts can be found here: https://github.com/Ruddernation-Designs/Adobe-URL-Block-List/blob/master/hosts
## Method A (Recommended)
1. Put **psunlock.exe** in your Photoshop folder
2. Create a desktop shortcut for **psunlock.exe**
3. Instead of launching the Photoshop you should launch the shortcut of PSUnlock.
PSUnlock automatically runs Photoshop. So you can use it as a launcher.

## Method B
1. Run **psunlock.exe** from any folder.
2. Run Photoshop. (You can launch **psunlock.exe** after Photoshop)

# How it works?
* When PS Unlock starts, it runs as a background process.
* While running, you'll see its icon in the system tray.
* Right-clicking the icon opens a context menu, where you can stop and exit PS Unlock.
* It instantly detects a running instance of Photoshop and begins monitoring for blocking popup windows.
* If Photoshop is not running, it will wait until you launch it.
* Double-clicking the tray icon will always bring the Photoshop window to the foreground (a quality-of-life feature).
* All blocking popup warnings are closed instantly. The Photoshop window becomes responsive, allowing you to continue your work without interruption.
* PSUnlock stops working right after you quit Photoshop.