# SSS

## SkiffLLM konuşmalarımı yüklüyor mu?

Hayır. Masaüstü çalışma zamanında ağ kodu yoktur. İstemler, geçmiş, ayarlar ve
üretilen metin makinede kalır. Android uygulaması hiçbir istemi hiçbir yere
göndermez.

## SkiffLLM model indiriyor mu?

Otomatik olarak hayır. Masaüstü çalışma zamanı bir GGUF dosyası gerektirir. İki
isteğe bağlı yardımcı vardır:

- Masaüstü: `python3 scripts/model_fetch.py --model qwen2.5-0.5b`
- Android: `Settings` → `Models` → `Download`

Her ikisi de Hugging Face'ten HTTPS kullanır ve açık kullanıcı eylemleridir.

## Hangi modeli kullanmalıyım?

Q4_K_M biçiminde küçük bir talimat GGUF'u ile başlayın:

- Qwen2.5-0.5B-Instruct
- Qwen3-0.6B-Instruct
- Llama-3.2-1B-Instruct

4 GB'den az RAM olan bir telefonda 0.5B veya 0.6B modellerini kullanın.

## İlk yanıtım neden yavaş?

Model yüklendikten sonraki ilk geçiş, istem işleme ve sayfa hatası ısınmasını
içerir. Masaüstünde `--warmup` kullanın veya gerçek bir konuşmadan önce Android
uygulamasını bir kez başlatın. Android uygulaması modeli yüklendikten sonra
otomatik olarak ısıtır.

## Oturumlar nerede saklanır?

Masaüstü: `--session`/`--history` oturum dizini (varsayılan olarak
`~/.local/share/skifflm` altında). Android: uygulama içi `conversation.json`;
uygulama verilerini temizlemek onu kaldırır.

## Yerel API'yi nasıl kullanıma açarım?

```bash
# yalnızca yerel
skifflm --model model.gguf --serve --host 127.0.0.1 --port 8080

# başka bir makineden erişilebilir, paylaşılan anahtarla korunur
skifflm --model model.gguf --serve --host 0.0.0.0 --port 8080 --api-key "local-token"
```

`--api-key` ayarlıyken `/v1/models` ve `/v1/chat/completions`
`Authorization: Bearer <key>` ister ve aksi halde `401` döner. `/health`,
`/version` ve `/` herkese açık kalır. Mümkün olduğunda varsayılan `127.0.0.1`
bağlantısını koruyun; yerel olmayan dinleyicilerde her zaman `--api-key`
ayarlayın.

## `--benchmark` neden satıcı rakamlarından farklı?

SkiffLLM'deki her benchmark, sağladığınız model dosyası ve donanımla kendi
makinenizde ölçülür. Rakamlar CPU/GPU'ya, örneklemeye, bağlam boyutuna, iş
parçacığı sayısına ve sistem yüküne bağlıdır. Sahte veya pazarlama rakamı yoktur.

## Android uygulaması ağ bağlantısı olmadan çalışır mı?

Evet, model yüklendikten sonra. Kayıtlı bir modeli yükleme, sohbet etme, dışa
aktarma ve temizleme çevrimdışı çalışır. Ağ yalnızca Hugging Face'ten yeni bir
model indirmeyi seçerseniz kullanılır.

## Telemetri etkin mi?

Hayır. Masaüstü çalışma zamanında veya Android uygulamasında analitik, çökme
raporlama veya kullanım izleme yoktur.
