$ErrorActionPreference = "Stop"

$taskName = "SARA_AutoStart"
$scriptPath = "C:\Users\utkarsh_kumar\Desktop\sara\bot\start_sara.bat"
$workingDir = "C:\Users\utkarsh_kumar\Desktop\sara"

Write-Host "Creating Scheduled Task '$taskName' to run at logon with highest privileges..."

$action = New-ScheduledTaskAction -Execute $scriptPath -WorkingDirectory $workingDir
$trigger = New-ScheduledTaskTrigger -AtLogOn
$principal = New-ScheduledTaskPrincipal -UserId $env:USERNAME -LogonType Interactive -RunLevel Highest

Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Principal $principal -Force

Write-Host "Task created successfully! SARA will now start silently on every boot without UAC prompts."
Start-Sleep -Seconds 3
