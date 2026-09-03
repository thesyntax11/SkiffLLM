# SkiffLLM ve neden Ollama değil?

Her yeni kullanıcının sorduğu soru budur ve net, dürüst bir cevabı hak ediyor.
SkiffLLM ile Ollama örtüşen sorunları çözer; doğru seçim dağıtım modelinize
bağlıdır.

Kısa özet: **Büyük bir model kataloğu ve tek komutla indirme isteyen akıcı bir
model sunucusu istiyorsanız Ollama'yı kullanın. Modelin native bir Unix aracı,
hava boşluklu bir bileşen veya CI/masaüstü/mobil iş akışına gömülü bir çıkarım
motoru gibi davranmasını istiyorsanız — çalışma zamanında daemon ve ağ olmadan —
SkiffLLM'i kullanın.**

## Karar matrisi

| Boyut | SkiffLLM | Ollama |
| --- | --- | --- |
| Çalışma zamanı ağ kullanımı | çıkarımda yok | varsayılan olarak indirme/pull servisi |
| Daemon / arka plan servisi | yok (tek süreç) | evet (`ollama serve`) |
| Model dosyası | kendi GGUF'unuz | yönetilen katalog, otomatik indirme |
| Model sabitleme / bütünlük | SHA-256 yan dosyası + `model verify` | kayıt başvuruları, daha az açık |
| Unix odaklı kullanım | native boru hatları, `--project`, `git` alt komutları | sunucu + istemci, boru öncelikli değil |
| Proje/kod bağlamı | yerleşik dosya indeksi + kaynak dilimi | harici olarak araçlarla, yerleşik değil |
| Native mobil istemciler | bu depoda Android + iOS | sunucu odaklı, topluluk arayüzleri |
| Hava boşluklu / çevrimdışı öncelik | açık hedef | yapılandırılabilir, varsayılan duruş değil |
| OpenAI uyumlu API | `--serve` (streaming, Bearer auth) | evet, birincil model |
| Denetleyici/motor ayrımı | derleme, llama.cpp arka uçlarını kontrol eder | paketlenmiş çalışma zamanı |
| Dosya/ayak izi | tek küçük binary | daemon + çalışma zamanı + model deposu |
| Model kataloğu genişliği | istediğiniz GGUF'u seçersiniz | geniş, kullanışlı |
| Ekosistem / GUI (Open WebUI vb.) | kendiniz kurarsınız | olgun |
| Sunucu sert sınırları | tek üretim, iç hız sınırı yok | daha kolay ölçeklenir, daha büyük ayak izi |

Hiçbir satır tek başına bir kusur değildir. Tablo tahmini ortadan kaldırmak
içindir.

## SkiffLLM'in açıkça daha iyi olduğu yerler

### 1. Hizmet değil Unix aracı istiyorsunuz

Ollama sunucu önceliklidir: bir hizmet çalıştırır ve HTTP API'siyle konuşursunuz.
SkiffLLM ise CLI önceliklidir:

```bash
git diff | skiffllm "review these changes"
cat error.log | skiffllm "find the root cause"
skiffllm --project . "where is authentication handled?"
skiffllm --code --project . "propose a fix for src/server.cpp"
```

Arka plan süreci, yönetilecek bir port veya kalıcılık kontrolü taşıyan konteyner
yoktur. `jq`, `xargs`, `git` ve cron ile diğer herhangi bir araç gibi birleşir.

### 2. Hava boşluklu veya çevrimdışı önceliklisiniz

Çalışma zamanı model indirmez ve ağ kodu yoktur. GGUF dosyasını siz getirirsiniz;
çıkarımı SkiffLLM yapar. Bu, hatırlamanız gereken bir ayar değil açık bir
duruştur. Ayrık veya kısıtlı ağlarda fark şudur: "varsayılan olarak çalışır"
ile "ağı kapattıktan sonra çalışır" arasındaki fark.

### 3. Çıkarım motoru üzerinde kontrol istiyorsunuz

SkiffLLM, seçtiğiniz llama.cpp sürümüne karşı derlenir. Arka uç
`SKIFFLLM_LLAMA_SOURCE_DIR` ve derleme arka uç bayrağıyla belirlenir;
`--backend-info` gerçekte neyin bağlandığını söyler. Derleyiciye, arka uca ve
binary'ye siz sahip olursunuz. Ollama bunu sizin için paketler ve yönetir; bu
kullanışlı ama daha az şeffaftır.

### 4. Tedarik zinciri kanıtı istiyorsunuz

`skiffllm model verify`, GGUF sihirli başlığını ve SHA-256 yan dosyasını denetler.
Katalog boyutu yalnızca bilgilendiricidir; yeni bir üst sürüm eski bayt sayısı
yüzünden reddedilmez. `model_fetch.py --checksum` yan dosyayı kaydeder ve
`--verify` yeniden indirmeden mevcut indirilen dosyayı kontrol eder. Bu,
denetimin istediği türden bir kanıttır: hangi model, hangi hash, nereden,
ne ölçüldü.

### 5. Aynı motordan mobil eşitlik istiyorsunuz

Depo, aynı model dosyalarına ve aynı çevrimdışı sözüne karşı native Android ve
iOS istemcilerini içerir. Bu Ollama'nın özelliği değildir; topluluk mobil
projeleri vardır ama çekirdek projenin parçası değildir.

### 6. Tekrarlanabilir, dürüst benchmark istiyorsunuz

`--benchmark` kendi makinenizde gerçek üretimler çalıştırır ve ölçülen istem
süresini, üretim süresini ve token/s değerlerini raporlar. Dokümanlar, bir sonuç
kabul edilmeden önce komut çıktısını ve model SHA-256'sını açıkça ister.
Pazarlama rakamı yoktur.

## Ollama'nın daha iyi olduğu yerler

- **Tek komutla model kurulumu.** `ollama pull llama3`, GGUF'u kendiniz bulup
  indirip doğrulamaktan daha basittir.
- **Katalog genişliği.** Resmi model kütüphanesi, manuel GGUF aramasından çok
  daha büyük ve gezinmesi kolaydır.
- **Sunucu öncelikli iş yükleri.** Birincil arayüz HTTP ise daemon modeli
  sorun değildir ve Open WebUI etrafındaki ekosistem olgundur.
- **Unix native nesne modeline ihtiyacınız yok.** Görev "ekibe yerel bir sohbet
  kutusu ver" ise Ollama daha düşük sürtünmeli yoldur.
- **Çok istekli eşzamanlılık.** Ollama'nın sunucusu çok sayıda istemciye hizmet
  etmek için kurulmuştur. SkiffLLM, llama.cpp bağlamı thread-safe olmadığı için
  üretimi bilinçli olarak tek mutex arkasında seri hale getirir.

## SkiffLLM hakkında dürüst çekinceler

- Model paketlemez; kurulum maliyeti yönetilen katalogdan yüksektir.
- `--serve` modu kompakt bir yerel sunucudur, çok iş parçacıklı bir ağ geçidi
  değildir: aynı anda bir üretim, iç hız sınırı yok, isteğe bağlı paylaşılan
  Bearer belirteci.
- Henüz büyük bir GUI ekosistemi yok.
- GPU donanımınızı destekleyen llama.cpp derleme arka ucunu doğrulamanız gerekir.

## Öneri

| Durum | Kullanın |
| --- | --- |
| Kabuk öncelikli, CI, git inceleme, çevrimdışı dizüstüler | SkiffLLM |
| Hava boşluklu / kısıtlı ağ | SkiffLLM |
| Masaüstü/mobil iş akışına gömülü | SkiffLLM |
| Tedarik zinciri sabitleme ve tekrarlanabilir benchmark | SkiffLLM |
| Büyük katalogla hızlı ekip sohbet kutusu | Ollama |
| Eşzamanlılıkla HTTP öncelikli hizmet | Ollama |

Alışkanlıktan değil, dürüstçe seçin. Karşılaştırıyorsanız özellik listesinden
değil, dağıtım modelinizden başlayın. Üretim dağıtımı için İngilizce
[docs/ENTERPRISE.md](../ENTERPRISE.md) ve Ollama alışkanlıklarının çevirisi için
[docs/OLLAMA_MIGRATION.md](../OLLAMA_MIGRATION.md) sayfalarına bakın.
