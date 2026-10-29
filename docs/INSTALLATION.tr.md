# SCC — Ready or Not kurulumu

## Desteklenen oyun sürümü

Mevcut başlatıcı Steam build `25784851` ve aşağıdaki EXE SHA-256 değerini bekler:

```text
FF445331E7144A2DC79EC50C2E6FD42F2A2E7B49AE7B03AE1A6076DCFC769262
```

Ayrıca `RoN-Internal/SDK/Basic.hpp` içindeki SDK sabitlerini okur; DLL'yi yüklemeden önce çalışan oyundaki `GObjects`, `GWorld` ve `ProcessEvent` değerlerini doğrular.

## Hazır sürümü kurma

1. GitHub Releases sayfasından `SCC-ReadyOrNot-<sürüm>.zip` dosyasını indir.
2. Arşivin tamamını çıkart. Başlatıcıyı, PowerShell betiğini, DLL'yi ve `RoN-Internal/SDK/Basic.hpp` yolunu birlikte tut.
3. Ready or Not'ı Steam üzerinden aç; ana menüyü bekle veya tek oyunculu/özel oturuma gir.
4. `SCC-ReadyOrNot-Aktif-Et.cmd` dosyasını çalıştır.
5. Başlatıcı şunları bildirmeli:
   - uyumluluk doğrulandı;
   - ImGui menüsü ve HUD bağlantısı doğrulandı.
6. SCC menüsünü açıp kapatmak için `Insert` tuşunu kullan.

Başlatıcı oyun EXE'sini veya kayıt dosyalarını diskte değiştirmez.

## Kontroller ve profil

- `Insert`: menüyü göster/gizle.
- Sistem → **Kaydet**: geçerli seçenekleri sakla.
- Sistem → **Yeniden yükle**: kaydedilmiş profili geri getir.
- Sistem → **Varsayılanlar**: güvenli varsayılanlara dön.
- Sistem → **Hileyi tamamen kaldır**: bu oturuma ait değişiklikleri geri verir, grafik/giriş bağlantılarını kaldırır ve DLL'yi boşaltır.

Profil ve kayıt DLL'nin yanında `SCC-ReadyOrNot.ini` ve `SCC-ReadyOrNot.log` olarak oluşturulur.

## SCC'yi güncelleme veya kaldırma

Menüdeki kaldırma düğmesini kullan veya şunu çalıştır:

```powershell
.\Activate-RoN.ps1 -Unload
```

DLL'yi değiştirmeden önce `KALDIRILDI` mesajını bekle. Oyun açık kalabilir. Projeyi tamamen silmek için SCC kaldırıldıktan sonra çıkartılan klasörü sil.

## Kaynak koddan derleme

MSVC v143 ve Windows SDK içeren Visual Studio 2022 Build Tools'u kur, ardından:

```powershell
git clone --recurse-submodules https://github.com/VallaBenyemedim/SCC-ReadyOrNot.git
cd SCC-ReadyOrNot
.\Build-RoN.ps1
.\Activate-RoN.ps1 -CheckOnly
```

Uyumluluk başarılıysa etkinleştir:

```powershell
.\Activate-RoN.ps1
```

## Sorun giderme

- **DLL bulunamadı:** Release x64 yapılandırmasını derle veya hazır sürüm ZIP'inin tamamını kullan.
- **Oyun sürümü uyuşmuyor:** zorla yükleme yapma; yeni build için SDK/adres güncellemesi gerekir.
- **Nesne tablosu / ProcessEvent uyuşmuyor:** canlı SDK düzeni bu sürümden farklıdır.
- **Oyun dünyası hazır değil:** ana menüyü bekle veya göreve girip yeniden dene.
- **HUD/menü doğrulanmadı:** `SCC-ReadyOrNot.log` dosyasını kontrol et; oyun penceresine geçip birkaç kare çizilmesini bekle.
- **Zaten aktif:** mevcut menüyü kullan; ikinci kopya özellikle engellenir.
- **Kontrol yetkisi yok:** oyun verisi yazmaları yerel oturum sahibi/yetkili oturum gerektirir; ESP bundan bağımsızdır.

SCC'yi çalıştırmak için Windows güvenlik kontrollerini kapatma. Hazır binary'ye güvenmiyorsan kaynak koddan derle.

## Sorumlu kullanım

SCC'yi yalnız çevrimdışı, tek oyunculu veya açıkça izin verilmiş özel oturumlarda kullan. Repo anti-cheat atlatma, genel sunucuya dağıtım veya rekabetçi kullanım desteği sağlamaz.
