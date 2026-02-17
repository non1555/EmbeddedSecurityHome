Option Explicit

Dim fso, shell, root, pyw, script
Set fso = CreateObject("Scripting.FileSystemObject")
Set shell = CreateObject("WScript.Shell")

root = fso.GetParentFolderName(WScript.ScriptFullName)
pyw = root & "\\.venv\\Scripts\\pythonw.exe"
script = root & "\\launcher.pyw"
Dim bootstrap
bootstrap = root & "\\bootstrap.cmd"

If Not fso.FileExists(pyw) Then
  If fso.FileExists(bootstrap) Then
    shell.Run "cmd.exe /c """ & bootstrap & """ --prepare-only", 1, True
  End If
End If

If Not fso.FileExists(script) Then
  MsgBox "Missing launcher script:" & vbCrLf & script, vbCritical, "EmbeddedSecurity LINE Bridge"
  WScript.Quit 2
End If

If Not fso.FileExists(pyw) Then
  MsgBox "Missing venv Python after bootstrap:" & vbCrLf & pyw, vbCritical, "EmbeddedSecurity LINE Bridge"
  WScript.Quit 2
End If

' Run hidden (0), do not wait (False)
shell.Run """" & pyw & """" & " " & """" & script & """", 0, False

