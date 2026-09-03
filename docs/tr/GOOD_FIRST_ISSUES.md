# Yeni Başlayanlar İçin İyi İlk Konular

Bu, projeye katkıda bulunmak için iyi başlangıç noktalarıdır. Her madde tamamlandığında
testlerin, dokümantasyonun ve üç platformun (masaüstü/CLI, Android, iOS) kapsamının
korunmasını gerektirir.

## Dokümantasyon

- Türkçe dokümanlarda eksik veya yanlış çevirileri düzeltin.
- `docs/tr/` ile `docs/` arasındaki içerik paritesini karşılaştırın ve farkları
  bildirin.
- Kurulum ve kullanım komutlarını `--help` çıktısıyla karşılaştırıp senkronizasyonu
  sağlayın.

## CLI

- Yeni bir komut için kısa devre için birim testi ekleyin.
- `--json` çıktılarında alan adlarının tutarlılığını doğrulayın.
- Hata mesajları için daha net yönlendirmeler ekleyin (ör. model yok).

## Android

- Model listesi ekranına bulunamayan `model.json` alanları için boş durumlar ekleyin.
- Yüklü bir modelin alt dosya listesini gösteren küçük bir ekran tasarlayın.
- İndirme sırasında iptal edilen durumlar için daha iyi geri bildirim ekleyin.

## iOS

- Masaüstü ve Android ile özellik eşitliğini karşılaştıran bir test sayfası ekleyin.
- `ChatView.swift`'i daha küçük bileşenlere ayırın.
- Kayıtlı oturumlar için görsel bir liste ekleyin.

## Genel kalite

- `scripts/ci-local.sh` ve `scripts/ci-android.sh` üzerindeki kontrolleri
  tamamlayın.
- `docs/benchmarks.md` için gerçek bir benchmark kaydı ekleyin (yalnızca kendi
  donanımınızda gerçek üretim yaparak).
- README'deki özellik tablosunu gerçek davranışla karşılaştırıp doğrulayın.
