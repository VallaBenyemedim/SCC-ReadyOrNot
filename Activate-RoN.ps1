param([switch]$CheckOnly, [switch]$Unload)
$ErrorActionPreference = 'Stop'
$taskDll = Join-Path $PSScriptRoot 'x64\Release\SCC-ReadyOrNot.dll'
$taskLog = [IO.Path]::ChangeExtension($taskDll, '.log')
$taskExpectedHash = 'FF445331E7144A2DC79EC50C2E6FD42F2A2E7B49AE7B03AE1A6076DCFC769262'

Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
public static class SCCReadyOrNotLoader {
 [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr OpenProcess(uint access, bool inherit, int id);
 [DllImport("kernel32.dll", SetLastError=true)] static extern bool ReadProcessMemory(IntPtr p, IntPtr a, byte[] b, UIntPtr size, out UIntPtr read);
 [DllImport("kernel32.dll", SetLastError=true)] static extern bool WriteProcessMemory(IntPtr p, IntPtr a, byte[] b, UIntPtr size, out UIntPtr written);
 [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr VirtualAllocEx(IntPtr p, IntPtr a, UIntPtr size, uint type, uint protection);
 [DllImport("kernel32.dll", SetLastError=true)] static extern bool VirtualFreeEx(IntPtr p, IntPtr a, UIntPtr size, uint type);
 [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr CreateRemoteThread(IntPtr p, IntPtr attributes, UIntPtr stack, IntPtr start, IntPtr arg, uint flags, out uint id);
 [DllImport("kernel32.dll", SetLastError=true)] static extern uint WaitForSingleObject(IntPtr h, uint timeout);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern IntPtr OpenEventW(uint access, bool inherit, string name);
 [DllImport("kernel32.dll", SetLastError=true)] static extern bool SetEvent(IntPtr h);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] static extern IntPtr GetModuleHandleW(string name);
 [DllImport("kernel32.dll", CharSet=CharSet.Ansi, ExactSpelling=true)] static extern IntPtr GetProcAddress(IntPtr h, string name);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool GetModuleHandleExW(uint flags, IntPtr address, out IntPtr h);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] static extern uint GetModuleFileNameW(IntPtr h, StringBuilder name, int length);
 static void Require(bool ok) { if (!ok) throw new Win32Exception(Marshal.GetLastWin32Error()); }
 public static IntPtr Open(int id, bool write) {
   var h = OpenProcess(write ? 0x43Au : 0x410u, false, id); Require(h != IntPtr.Zero); return h;
 }
 public static void Close(IntPtr h) { CloseHandle(h); }
 public static void RequestStop(int id) {
   IntPtr stop = OpenEventW(2, false, "Local\\SCCReadyOrNot.Stop." + id);
   if (stop == IntPtr.Zero) throw new Exception("Bu kopyada menu kaldirma baglantisi yok. Eski surum icin F11 kullanin.");
   try { Require(SetEvent(stop)); } finally { CloseHandle(stop); }
 }
 public static byte[] Read(IntPtr h, long address, int size) {
   byte[] b = new byte[size]; UIntPtr n;
   Require(ReadProcessMemory(h, new IntPtr(address), b, (UIntPtr)size, out n));
   if (n.ToUInt64() != (ulong)size) throw new Exception("Eksik bellek okuma.");
   return b;
 }
 public static long Pointer(IntPtr h, long address) { return BitConverter.ToInt64(Read(h,address,8),0); }
 public static string LoadLibraryOwner(out long offset) {
   IntPtr fn = GetProcAddress(GetModuleHandleW("kernel32.dll"), "LoadLibraryW"), owner;
   Require(fn != IntPtr.Zero); Require(GetModuleHandleExW(6,fn,out owner));
   offset = fn.ToInt64() - owner.ToInt64();
   StringBuilder path = new StringBuilder(32768); Require(GetModuleFileNameW(owner,path,path.Capacity) != 0);
   return System.IO.Path.GetFileName(path.ToString());
 }
 public static void Load(IntPtr h, long loadLibraryAddress, string path) {
   byte[] b = Encoding.Unicode.GetBytes(path + "\0");
   IntPtr remote = VirtualAllocEx(h, IntPtr.Zero, (UIntPtr)b.Length, 0x3000, 4);
   Require(remote != IntPtr.Zero);
   IntPtr thread = IntPtr.Zero; bool finished = true;
   try {
     UIntPtr n; Require(WriteProcessMemory(h,remote,b,(UIntPtr)b.Length,out n));
     if (n.ToUInt64() != (ulong)b.Length) throw new Exception("Eksik yol yazimi.");
     uint id; thread = CreateRemoteThread(h,IntPtr.Zero,UIntPtr.Zero,new IntPtr(loadLibraryAddress),remote,0,out id);
     Require(thread != IntPtr.Zero); finished = false;
     uint wait = WaitForSingleObject(thread,15000);
     if (wait != 0) throw new Exception("DLL yukleme zaman asimi. Oyunun durumunu kontrol edin.");
     finished = true;
   } finally {
     if (thread != IntPtr.Zero) CloseHandle(thread);
     if (finished) VirtualFreeEx(h,remote,UIntPtr.Zero,0x8000);
   }
 }
}
'@

try {
 if (![Environment]::Is64BitProcess) { throw '64 bit PowerShell gerekiyor.' }
 if ($CheckOnly -and $Unload) { throw '-CheckOnly ve -Unload birlikte kullanilamaz.' }
 $taskGame = Get-Process -Name 'ReadyOrNotSteam-Win64-Shipping' -ErrorAction SilentlyContinue
 if ($Unload) {
   if (!$taskGame) { Write-Host 'Oyun kapali; kaldirilacak aktif kopya yok.'; exit 0 }
   if (@($taskGame).Count -ne 1) { throw 'Tek bir oyun islemi gerekiyor.' }
   $taskGame.Refresh()
   if (!@($taskGame.Modules | Where-Object ModuleName -eq 'SCC-ReadyOrNot.dll').Count) { Write-Host 'Aktif kopya yok.'; exit 0 }
   [SCCReadyOrNotLoader]::RequestStop($taskGame.Id)
   $taskDeadline = (Get-Date).AddSeconds(15)
   do {
     Start-Sleep -Milliseconds 200
     $taskGame.Refresh()
     $taskLoaded = @($taskGame.Modules | Where-Object ModuleName -eq 'SCC-ReadyOrNot.dll').Count -gt 0
   } until (!$taskLoaded -or (Get-Date) -gt $taskDeadline)
   if ($taskLoaded) { throw 'Kaldirma istegi gonderildi; temizlik henuz bitmedi. Calisma kaydini kontrol edin.' }
   Write-Host 'KALDIRILDI: menu, ESP ve baglantilar temizlendi.'
   exit 0
 }
 if (!(Test-Path -LiteralPath $taskDll)) { throw 'DLL bulunamadi. Once Build-RoN.ps1 calistirin.' }
 if (!$taskGame -and !$CheckOnly) {
   Write-Host 'Ready or Not baslatiliyor...'
   Start-Process 'steam://rungameid/1144200'
   $taskDeadline = (Get-Date).AddSeconds(120)
   do {
     Start-Sleep -Seconds 2
     $taskGame = Get-Process -Name 'ReadyOrNotSteam-Win64-Shipping' -ErrorAction SilentlyContinue
   } until ($taskGame -or (Get-Date) -gt $taskDeadline)
 }
 if (!$taskGame -or @($taskGame).Count -ne 1) { throw 'Tek bir Ready or Not Steam islemi bulunamadi.' }
 if ((Get-FileHash -LiteralPath $taskGame.Path -Algorithm SHA256).Hash -ne $taskExpectedHash) {
   throw 'Oyun surumu bu derlemeyle uyusmuyor. DLL yuklenmedi; adresler yeniden guncellenmeli.'
 }
 $taskSource = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'RoN-Internal\SDK\Basic.hpp') -Raw
 $taskOffsets = @{}
 foreach ($taskMatch in [regex]::Matches($taskSource,'constexpr int32\s+(\w+)\s*=\s*(0x[\dA-Fa-f]+)')) {
   $taskOffsets[$taskMatch.Groups[1].Value] = [Convert]::ToInt64($taskMatch.Groups[2].Value.Substring(2),16)
 }
 $taskBase = $taskGame.MainModule.BaseAddress.ToInt64()
 $taskHandle = [SCCReadyOrNotLoader]::Open($taskGame.Id, !$CheckOnly)
 try {
   $taskObjects = [SCCReadyOrNotLoader]::Read($taskHandle,$taskBase+$taskOffsets.GObjects,32)
   $taskChunks = [BitConverter]::ToInt64($taskObjects,0)
   $taskCount = [BitConverter]::ToInt32($taskObjects,20)
   $taskMaximum = [BitConverter]::ToInt32($taskObjects,16)
   if ($taskChunks -lt 0x10000 -or $taskCount -lt 1000 -or $taskCount -gt $taskMaximum) { throw 'Nesne tablosu uyumsuz.' }
   $taskFirst = [SCCReadyOrNotLoader]::Pointer($taskHandle,[SCCReadyOrNotLoader]::Pointer($taskHandle,$taskChunks))
   $taskVtable = [SCCReadyOrNotLoader]::Pointer($taskHandle,$taskFirst)
   if ([SCCReadyOrNotLoader]::Pointer($taskHandle,$taskVtable+8*$taskOffsets.ProcessEventIdx) -ne $taskBase+$taskOffsets.ProcessEvent) { throw 'ProcessEvent uyumsuz.' }
   $taskDeadline = (Get-Date).AddSeconds(60)
   do {
     $taskWorld = [SCCReadyOrNotLoader]::Pointer($taskHandle,$taskBase+$taskOffsets.GWorld)
     if ($taskWorld -gt 0x10000 -or $CheckOnly) { break }
     Start-Sleep -Seconds 1
   } until ((Get-Date) -gt $taskDeadline)
   if ($taskWorld -lt 0x10000) { throw 'Oyun dunyasi henuz hazir degil; ana menu acildiginda yeniden deneyin.' }
   Write-Host "Uyumluluk dogrulandi: Steam build 25784851, PID $($taskGame.Id), $taskCount nesne."
   if ($CheckOnly) { exit 0 }
   $taskGame.Refresh()
   $taskExisting = @($taskGame.Modules | Where-Object ModuleName -eq 'SCC-ReadyOrNot.dll')
   if ($taskExisting.Count) {
     if ($taskExisting[0].FileName -ne $taskDll) { throw 'Baska bir SCC-ReadyOrNot kopyasi zaten yuklu.' }
     Write-Host 'SCC-ReadyOrNot zaten aktif.'
     exit 0
   }
   $taskOffset = [long]0
   $taskOwner = [SCCReadyOrNotLoader]::LoadLibraryOwner([ref]$taskOffset)
   $taskOwnerModule = $taskGame.Modules | Where-Object ModuleName -eq $taskOwner
   if (!$taskOwnerModule) { throw 'Windows yukleyici modulu bulunamadi.' }
   [SCCReadyOrNotLoader]::Load($taskHandle,$taskOwnerModule.BaseAddress.ToInt64()+$taskOffset,$taskDll)
 } finally { [SCCReadyOrNotLoader]::Close($taskHandle) }
 $taskDeadline = (Get-Date).AddSeconds(20)
 do {
   Start-Sleep -Milliseconds 300
   $taskGame.Refresh()
   $taskLoaded = @($taskGame.Modules | Where-Object { $_.FileName -eq $taskDll }).Count -gt 0
   $taskContent = if (Test-Path -LiteralPath $taskLog) { Get-Content -LiteralPath $taskLog -Raw } else { '' }
   if ($taskLoaded -and $taskContent -match 'Hooked ProcessEvent on Active HUD successfully' -and $taskContent -match '\[UI\] Menu rendering active') { break }
 } until ((Get-Date) -gt $taskDeadline)
 if (!$taskLoaded) { throw 'DLL oyun surecine yuklenemedi.' }
 if ($taskContent -notmatch 'Hooked ProcessEvent on Active HUD successfully') { throw "DLL yuklendi; HUD baglantisi henuz dogrulanamadi. Kayit: $taskLog" }
 if ($taskContent -notmatch '\[UI\] Menu rendering active') { throw "ImGui cizimi henuz dogrulanamadi. Kayit: $taskLog" }
 Write-Host 'AKTIF: ImGui menusu ve oyun HUD baglantisi dogrulandi.'
 Write-Host 'INSERT: menuyu ac/kapat | Tum ayarlar menude | Sistem > Hileyi tamamen kaldir'
 Write-Host "Kayit: $taskLog"
 exit 0
} catch {
 Write-Host "HATA: $($_.Exception.Message)" -ForegroundColor Red
 exit 1
}
