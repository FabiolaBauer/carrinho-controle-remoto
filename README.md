# Carrinho Controle Remoto – Firmware (ESP‑IDF v6.1)

## Visão geral
Este repositório contém o firmware para o **carrinho** (ESP32 #2) da disciplina de Sistemas Embarcados. O código segue a especificação _ESPECIFICACAO_CODIGO.md_ e foi desenvolvido usando **ESP‑IDF v6.1**.

---

## Estrutura do projeto
```
trabalhoGB/
├─ components/                # protocolos e lógica pura (C puro)
│   ├─ protocol/               # estruturas de pacote, CRC‑8
│   └─ logic/                  # joystick, mixer, rampa, link, state‑machine
├─ car/                        # aplicação IDF do carrinho
│   ├─ main/                  # firmware principal
│   │   ├─ main.c
│   │   ├─ bench.c
│   │   ├─ bench.h
│   │   ├─ config.h
│   │   └─ …
│   ├─ CMakeLists.txt
│   └─ sdkconfig.defaults
├─ controller/                # esqueleto do firmware do controlador (não implementado)
├─ test_apps/logic_host/      # testes Unity (Linux host)
└─ README.md                  # ← este arquivo
```

---

## Versão do ESP‑IDF
Recomendado **v6.1**. Verifique com:
```powershell
idf.py --version
```
Se a versão for diferente, **pare** e ajuste o ambiente.

---

## Como compilar e gravar
1. Abra o *ESP‑IDF PowerShell* (ou execute `export.ps1`).
2. Navegue ao diretório `car`:
   ```powershell
   cd C:\Users\Nathan\Documents\trabalhocarrinho\carrinho-controle-remoto\carrinho
   ```
3. **Compilar (modo normal)**:
   ```powershell
   idf.py build
   ```
4. **Compilar no modo de bancada (bench mode)** – habilita `CONFIG_CAR_BENCH_MODE` e desativa Wi‑Fi/ESP‑NOW:
   ```powershell
   idf.py -D SDKCONFIG_DEFAULTS='sdkconfig.defaults;sdkconfig.defaults.bench' build
   ```
5. **Flash** (substitua `COMx` pela porta correta):
   ```powershell
   idf.py -p COMx flash monitor
   ```
   O monitor exibirá mensagens de inicialização, incluindo um aviso quando o **modo de bancada** estiver ativo.

---

## Modo de bancada (Bench Mode)
- **Ativado** via `sdkconfig.defaults.bench` (`CONFIG_CAR_BENCH_MODE=y`).
- Wi‑Fi e ESP‑NOW são *não inicializados*.
- Uma tarefa (`bench_task`) injeta sequências de pacotes de comando na fila de recepção (`s_rx_queue`).
- A sequência padrão está definida em `bench.c` (9 passos, 20 Hz). Cada passo tem duração, valores de `x`/`y` e descrição legível nos logs.
- Logs de benchmark são emitidos com `ESP_LOGI`/`ESP_LOGW` indicando início, cada step e finalização.
- Ideal para testes em bancada com o carrinho levantado (rodas livres).

### Como usar
1. Habilite o modo de bancada (passo 4 acima).  
2. Compile e flash.
3. Observe o terminal — as mensagens `EVT bench_step` aparecerão a cada step.
4. Para interromper a sequência, reinicie o dispositivo.

---

## Checklist de compilação (fase 1)
- [x] `idf.py build` termina **sem warnings**.
- [x] `test_apps/logic_host` compila e todos os testes relevantes passam.
- [x] Modo de bancada está implementado e documentado.
- [ ] Teste em hardware (não realizado neste ambiente).

---

## APIs validadas
| API | Fonte | Data | Observação |
|-----|-------|------|------------|
| `nvs_flash_init` | ESP‑IDF v6.1 docs | 2026‑11‑12 | ✅ |
| `esp_wifi_*` | ESP‑IDF v6.1 docs | 2026‑11‑12 | ✅ |
| `esp_now_*` | ESP‑IDF v6.1 docs | 2026‑11‑12 | ✅ |
| `esp_timer_get_time` | ESP‑IDF v6.1 docs | 2026‑11‑12 | ✅ |
| `xQueueCreate / xQueueSend / xQueueReceive` | FreeRTOS v10.5 (bundled) | 2026‑11‑12 | ✅ |
| `crc8_calc` (custom) | `components/protocol` | 2026‑11‑12 | ✅ |
| `bench_mode_start` | Implementado neste repo | 2026‑11‑12 | ✅ |

---

## Suposições
- Valores de hardware (pinos, limites) estão em `config.h` conforme a seção 5 da especificação.
- O controlador ainda não existe; por isso o carrinho usa apenas o modo de bancada para testes.
- Todos os números “mágicos” foram movidos para `config.h`.
- O código foi compilado apenas em **Windows**; caminhos foram validados para esse SO.

---

## Como contribuir / Próximos passos
- Implementar o firmware do controlador (esqueleto já está presente).
- Adicionar opções de configuração de benchmark via `Kconfig` (delay, número de passos, etc.).
- Testar em hardware real e validar tempo de resposta.

---

*Este README foi gerado automaticamente a partir das instruções da especificação e das alterações recentes do código.*