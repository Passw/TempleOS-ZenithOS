#requires -RunAsAdministrator
param ($target="create", [uint64]$diskSize=2GB, $driveLetter='Z')

$ql2Url = "https://github.com/qloader2/qloader2"
$vhdPath = ".\ZenithOS.vhd"
$vboxPresent = Get-Command "VBoxManage" -ErrorAction SilentlyContinue

function Test-HyperVInstalled
{#You cannot enable _just_ the powershell module and services feature using `Enable-WindowsOptionalFeature`, it only does the whole package.
 #We have to do it by hand, not really that much of an issue. :^|
 #Enabling the whole package enables the hypervisor and forces any other virtualization software like VirtualBox to use its slow Native API.
 #We don't want that, we just want the VHD image tools.
	if ((Get-WindowsEdition -Online).Edition -eq "Home")
	{
		Write-Host -ForegroundColor Red `
		"You are running Windows 10 Home. Hyper-V is not supported on this edition of Windows. Please upgrade to Professional."
		exit
	}

	if (-not (Get-Module -ListAvailable -Name "Hyper-V"))
	{
		Write-Host -ForegroundColor Red "`nError: Hyper-V module not installed.`n"
		Write-Host "The following Windows Features need to be enabled under " -NoNewLine
		Write-Host -ForegroundColor Yellow "Hyper-V" -NoNewLine
		Write-Host ":`n"
		Write-Host -ForegroundColor Black -BackgroundColor Yellow "Hyper-V Module for Windows PowerShell" -NoNewLine
		Write-Host "  " -NoNewLine
		Write-Host -ForegroundColor Black -BackgroundColor Yellow "Hyper-V Services"

		$yes = New-Object System.Management.Automation.Host.ChoiceDescription "&Yes", "Launch the Windows Features dialog and exit."
		$no = New-Object System.Management.Automation.Host.ChoiceDescription "&No", "Exit. You can launch the Windows Features dialog yourself later."
		$options = [System.Management.Automation.Host.ChoiceDescription[]]($yes, $no)

		if ($Host.UI.PromptForChoice("", "`nLaunch the Windows Features dialog to enable them?", $options, 0) -eq 0)
		{
			Start-Process "OptionalFeatures"
		}
		exit
	}
}

Test-HyperVInstalled

function Get-Deps
{
	if (-not (Test-Path ".\qloader2"))
	{
		git clone $ql2Url
	}
}

function Update-Deps { git -C .\qloader2 pull }

function New-FAT32VHD
{#Create New™ MBR FAT32 formatted VHD.
	$uuid = $null
	if (Test-Path $vhdPath)
	{
		if ($vboxPresent)
		{	#This is quite a hack.
			#If VBoxManage returned an object with properties like a Powershell module it would be way better.
			$uuid = (VBoxManage showhdinfo $vhdPath | 
					Select-String UUID |
					Select-String -NotMatch Parent |
					Select-String -NotMatch Error |
					Select-String -NotMatch VM |
					Out-String).trim().substring("UUID:".length).trim()
#			Write-Host $uuid
		}
		Remove-Item $vhdPath
	}
	New-VHD $vhdPath -SizeBytes $diskSize -Fixed  |
	Mount-VHD -Passthru -NoDriveLetter            |
	Initialize-Disk -PartitionStyle MBR -Passthru |
	New-Partition -MbrType FAT32 -UseMaximumSize  |
	Format-Volume -FileSystem FAT32 -Force        |
	Get-Partition								  |
	Set-Partition -NewDriveLetter $driveLetter

	Write-Host "Created disk, size" ($diskSize / 1MB) MiB.

	Copy-Item -Recurse -Force src\* ${driveLetter}:\

	Dismount-VHD $vhdPath

	if (Test-Path "qloader2\qloader2-install.exe")
	{
		qloader2\qloader2-install.exe qloader2\qloader2.bin $vhdPath
		Write-Host "Installed qloader2 to $vhdPath"
	}
	else { Write-Host -ForegroundColor Red "Warning: qloader2-install executable was not found. Bootloader not installed." }

	if ($vboxPresent)
	{
		VBoxManage internalcommands sethduuid $vhdPath $uuid
	}
	else { Write-Host "VBoxManage was not found in PATH. You will have to deal with UUID conflicts manually." }
}

function Mount-FAT32VHD
{
	Mount-VHD $vhdPath -NoDriveLetter -Passthru |
	Get-Partition 								|
	Set-Partition -NewDriveLetter $driveLetter
}

function Export-FAT32VHD
{
	Mount-FAT32VHD
	Copy-Item -Recurse -Force ${driveLetter}:\* src\
	Dismount-VHD $vhdPath
}

function Start-VBoxVM
{
	if ($vboxPresent)
	{
		$uuid = (VBoxManage showhdinfo $vhdPath | Select-String "In Use by VM" | Out-String).trim()
		VBoxManage startvm $uuid.substring($uuid.length - 37, 36)
	}
	else { Write-Host -ForegroundColor Red "VBoxManage not found on PATH." }
}

switch ($target)
{
    "create" 		{ New-FAT32VHD } #create <size> <driveLetter-to-use>
	"export"		{ Export-FAT32VHD }
	"mount"			{ Mount-FAT32VHD }
	"dismount"		{ Dismount-VHD $vhdPath }
	"get-deps"		{ Get-Deps }
	"update-deps"	{ Update-Deps }
	"run"			{ Start-VBoxVM }
    default 		{ Write-Host -ForegroundColor Red "Error: Unknown target '$_'." }
}
