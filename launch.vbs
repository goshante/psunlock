Set WshShell = CreateObject("WScript.Shell")
WshShell.Run """psunlock.exe""", 0, False
WshShell.Run """Photoshop.exe""", 0, False