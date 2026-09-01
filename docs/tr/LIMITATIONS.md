# Bilinen Sınırlamalar

Bu liste bilinçli olarak dürüsttür. Projenin mevcut durumunu tanımlar.

## Model dosyaları

SkiffLLM model paketlemez. Masaüstü çalışma zamanı var olan bir GGUF dosyası
gerektirir; bu sayede kendi çalışma zamanı ağ kullanımı sıfır olur, ancak
alma (retrieval) içeren hizmetlere göre kurulum maliyeti artar.

İki açık seçenek vardır:

- Masaüstü: `python3 scripts/model_fetch.py --model <id>`
- Android: `Settings` → `Models` → `Download`

Her ikisi de Hugging Face'ten kullanıcı başlatmalı HTTPS aktarımlarıdır. Android
uygulaması `INTERNET` iznini yalnızca bu amaç için kullanır.

## Yerel API sunucusu

`--serve` modu kompakt, yerel bir HTTP sunucusudur. Çok iş parçacıklı bir üretim
ağ geçidi değildir.

- Hızlı uç noktalar (`/health`, `/version`, `/v1/models`) bir sohbet üretimi
  çalışırken yanıt verir; llama.cpp bağlamı iş parçacığı güvenli olmadığı için
  üretim bir mutex arkasında seri hale getirilir.
- `/v1/*` uç noktaları isteğe bağlı `--api-key` kabul eder ve ardından
  `Authorization: Bearer <key>` ister. Anahtar ayarlı değilken herkese açıktır;
  bu yalnızca `127.0.0.1` üzerinde dinlerken güvenlidir.
- Hız sınırlaması yoktur.
- Yerel araçlar, editör eklentileri ve kişisel otomasyon için tasarlanmıştır.

## Bağlam

Üretim, yapılandırılan bağlam boyutuyla sınırlıdır. Çok uzun konuşmalar yalnızca
`--auto-trim` etkinleştirildiğinde kırpılır; aksi halde istem çok büyük olduğunda
hata üretilir.

## İstem şablonları

Sohbet şablonu geçersiz kılma, yüklü llama.cpp modelinin desteklediği bir adı
kabul eder. Bilinmeyen şablon adları `chatml`'e geri döner veya net bir hatayla
başarısız olur.

## Android

- Aynı anda bir üretim ve bir indirme çalıştırır.
- Uygulama modelleri UI benzeri arka plan yürütücüsünde ısıtır ve yükler; büyük
  modeller yine de zaman ve bellek alır.
- Model indirmeleri yeterli boş depolama ve ağ bağlantısı gerektirir.
- GPU boşaltma, llama.cpp derleme desteğine ve cihaz arka uçlarına bağlıdır.

## CI

CI ve yayın iş akışı dosyaları çalışma ağacında bulunur ancak depo gerekli
`workflows` iznini verene kadar bu dala gönderilmez. O zamana kadar yerel
kontroller `scripts/ci-local.sh` (masaüstü) ve `scripts/ci-android.sh` (Android
SDK kuruluysa Android) ile çalıştırılabilir.

## Planlananlar

- Embedding'ler ve alma-artırılmış üretim
- Birden çok model arasında çok işçili üretim
- Paket yöneticisi entegrasyonu
- Dil bilgisi kısıtlı üretim
- Arka uç benchmark matrisi
