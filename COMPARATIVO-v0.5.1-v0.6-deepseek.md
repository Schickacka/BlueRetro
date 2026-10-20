# Comparativo técnico — BlueRetro PS1 Custom Edition

## v0.5.1 → v0.6-deepseek

**Para:** time de desenvolvimento  
**Base analisada:** BlueRetro v0.5.1 — fork PlayStation 1 Custom Edition  
**Entrega:** v0.6-deepseek  
**Ambiente de referência:** ESP-IDF v5.5.1, `xtensa-esp32-elf` 14.2.0, ESP32, alvo PS1  
**Status:** compilado e validado estaticamente; **homologação em hardware pendente**  
**Documentos irmãos:** `ANALISE-v0.6-deepseek.md` (auditoria completa), `PACOTE-PARA-DEVS.md` (pacote), `patches/v0.6-deepseek.patch` (diff formal)

---

## 0. Como ler este documento

- **v0.5.1** é a árvore original em que o projeto foi baseado.
- **v0.6-deepseek** é a árvore entregue.
- **Preservado** significa comportamento intencional da v0.5.1 que **não** foi alterado.
- **Corrigido** significa defeito real ou risco concreto identificado na revisão.
- **Adicionado** significa recurso novo, sempre com padrão seguro.
- **Desativado** significa recurso retirado do build padrão por decisão de arquitetura.
- **Motivo** é a razão técnica ou de produto; **Validação** é o que já foi feito e o que falta.

O patch formal contém a diferença exata. Este documento explica **por que** cada diferença existe.

---

## 1. Resumo executivo

A v0.6-deepseek não é um redesign. É uma **correção de robustez, concorrência e arquitetura** sobre a v0.5.1, mantendo as quatro personalizações do fork:

1. ESP32 invisível sem controle Bluetooth;
2. emulação 1:1 do controle com fio;
3. blink azul N piscadas + pulso de conexão;
4. barra vermelha no analógico e cinza no digital.

Além disso, a v0.6:

- corrige o layout/flags do DualSense, que impedia barra e player LEDs de funcionarem corretamente;
- separa buffers HID/HCI e serializa os caminhos concorrentes;
- corrige o feedback do Switch/8BitDo;
- troca os timers auto-destrutivos de blink/pulso por máquinas de estado no tick de 10 ms;
- corrige o flush do memory card e o mapeamento de multitap;
- resolve o CCOUNT entre núcleos;
- limita a espera da fila TX para evitar deadlock;
- cria o build PS1-only;
- adiciona um menu Kconfig com os recursos novos;
- **desliga o cartão virtual no ESP32 principal**; o memory card está fora do escopo desta versão e o foco é o controle;
- adiciona estatísticas e probe de latência opcionais;
- adiciona documentação, patch formal e empacotamento reproduzível.

### Números principais

| Métrica | v0.5.1 (todos os sistemas) | v0.6-deepseek padrão (PS1-only, sem cartão) | v0.6 opcional com cartão |
|---|---:|---:|---:|
| Flash da aplicação | 605.616 B (`0x93db0`) | **550.640 B (`0x866f0`)** | 551.984 B (`0x86c30`) |
| Flash livre (partição 1 MB) | 42% | **47%** | 47% |
| IRAM usada | 129.763 B (1.309 B livres) | **106.135 B (24.937 B livres)** | 106.287 B (24.785 B livres) |
| DRAM usada | 57.452 B | **46.524 B (78.056 B livres)** | 46.660 B (77.920 B livres) |
| Heap do cartão virtual | 128 KB no boot | **0** | 128 KB |
| Cartão virtual no ESP32 principal | Sim | **Não** (fora do escopo) | Sim, opcional |

**Leitura:** o build padrão libera ~54,9 KB de flash, ~23,6 KB de IRAM e 128 KB de heap em relação à v0.5.1 completa. O PS2 foi mantido; o cartão virtual está fora do escopo; o foco é o controle.

---

## 2. Baseline: o que a v0.5.1 já tinha

A v0.5.1 já era um fork funcional, com as personalizações do projeto:

- `hotplug = 1` e gate de `BT_DEV_HID_INIT_DONE`;
- deadzone LF `0x40`, saturação HF `0xFF` e remoção do gate `acc_mode & ACC_RUMBLE` no comando `0x42`;
- blink azul N piscadas no DS4/clone e pulso de conexão de 150 ms;
- barra vermelha no analógico (`0x6D`) e cinza no digital (`0x101010`);
- suporte a DS4/DS5, Switch/8BitDo, PS3, Wii;
- adaptador PS1/PS2 compartilhado;
- cartão virtual PS1 de 128 KB;
- build universal com todos os consoles wired.

Os defeitos encontrados estavam em **como** esses recursos eram implementados, não na intenção deles.

---

## 3. O que foi preservado de propósito

| Item | v0.5.1 | v0.6-deepseek | Motivo |
|---|---|---|---|
| ESP32 invisível sem controle BT | `hotplug = 1` + gate de init | **Mantido** | Requisito do projeto: as duas portas ficam livres para controles com fio. |
| Deadzone LF `0x40` | Sim | **Mantido** | Emulação 1:1 do DualShock; validado em THPS2/RE3/Alien. |
| Saturação HF `0xFF` | Sim | **Mantido** | O motor leve é binário; o valor correto é 0xFF. |
| Sem gate `acc_mode & ACC_RUMBLE` no `0x42` | Sim | **Mantido** | O feedback do jogo não deve depender do modo de acessório. |
| Blink azul N piscadas | Sim, via `esp_timer` | **Mantido no comportamento**, reimplementado no tick de 10 ms | Evitar timer auto-destrutivo; mesma aparência. |
| Pulso de conexão 150 ms | Sim, via `esp_timer` | **Mantido no comportamento**, reimplementado no tick | Evitar timer auto-destrutivo; save/restore dos motores. |
| Barra vermelha/cinza | Sim | **Mantida**, agora configurável no Kconfig | Requisito visual do SCPH-1200. |
| DS5 com player LEDs + barra | Parcial (layout errado) | **Corrigido** | O comportamento desejado era esse; a v0.5.1 não o entregava por causa do layout. |
| Controles wireless | Sim | **Mantidos** | São entradas de controle, não sistemas de console; removê-los quebraria DS4/DS5/Switch/PS3/Wii/8BitDo. |
| PS2 compartilhado | Sim | **Mantido** | `ps.c`/`ps_spi.c` são comuns à família PlayStation; o corte não compensa. |
| Clone `0x05` no canal de interrupção | Decisão do fork | **Mantido**, com o mesmo risco residual documentado | É uma decisão de compatibilidade com clones; precisa de teste com clones reais. |
| `bt_host_q_wait_pkt(60)` | Sim | **Mantido** | Caminho de autenticação/pareamento do fork. |
| Testes de debug (`ADAPTER_RUMBLE_DBG`, etc.) | Sim | **Mantidos** | Não afetam o build de produção. |

---

## 4. Correções críticas

### 4.1 DualSense: layout e flags do report 0x02

**v0.5.1:** `struct bt_hidp_ps5_set_conf` tinha um byte extra (`use_accurate_rumble`) antes de `valid_flag2`. Isso deslocava `valid_flag2`, `lightbar_setup`, `led_brightness` e `player_leds`; o DS5 recebia flags no lugar errado e a barra/player LEDs podiam não funcionar. O `conf1` era `0x04` e o `valid_flag2` era `0x14`.

**v0.6:** o byte extra foi removido; o layout segue o report USB 0x02:

- `valid_flag2` em 39;
- `reserved3[2]` em 40–41;
- `lightbar_setup` em 42;
- `led_brightness` em 43;
- `player_leds` em 44;
- `conf1 = 0x14` (lightbar `0x04` + player indicator `0x10`);
- `valid_flag2 = 0x02`;
- `lightbar_setup = 0x02`;
- `led_brightness = 0x02` (ou `0x01` no night mode).

Também foi criado `bt_hidp_ps5_player_led()` com **5 padrões** (1 a 5 LEDs), com clamp para os jogadores 6–7.

**Motivo:** o DS5 precisava dos flags no offset correto para aceitar a barra e os player LEDs. O comportamento desejado (LEDs do próprio controle, barra cinza/vermelha, sem blink azul) não era confiável na v0.5.1.

**Arquivos:** `main/bluetooth/hidp/ps.h`, `main/bluetooth/hidp/ps.c`, `main/adapter/wireless/ps.c`.  
**Validação:** compilação e inspeção do layout; **teste visual no DS5 pendente**.

### 4.2 Buffer HID/HCI compartilhado

**v0.5.1:** o feedback HID (DS4/DS5/Switch/Wii/PS3) e os comandos HCI/ATT/L2CAP/SDP/SMP montavam pacotes no mesmo `bt_hci_pkt_tmp`. A tarefa de feedback e o host BT podiam sobrescrever o header/CRC um do outro.

**v0.6:** foi criado `bt_hid_pkt_tmp`, dedicado ao HID. O `bt_hci_pkt_tmp` continua para HCI/ATT. O mutex recursivo `bt_tx_mutex` serializa o enfileiramento; os caminhos de RX, SDP, inquiry/disconnect e reset também entram no lock.

**Motivo:** eliminar a corrida que corrompia o CRC dos relatórios do DS4/DS5 e podia derrubar o feedback.

**Arquivos:** `hidp.c`, `hidp.h`, `generic.c`, `wii.c`, `ps3.c`, `sw.c`, `ps.c`, `hci.c`, `host.c`.  
**Validação:** compilação; **teste de feedback sob carga pendente**.

### 4.3 Switch/8BitDo: marcador `output[127]`

**v0.5.1:** o marcador do tipo de feedback (`output[127]`) só era gravado em `FB_TYPE_RUMBLE`. Um evento de LED com rumble ativo podia perder o LED, e o caminho de init podia ler além de um struct local.

**v0.6:**

- o marcador é gravado em **todo** feedback;
- o `bt_hid_cmd_sw_set_conf()` verifica se o report é o buffer persistente (`is_fb`) antes de ler `[127]`;
- se um LED chega com rumble ativo, envia `SET_CONF` (LED) e em seguida `SET_RUMBLE` (LRA) para não perder nenhum dos dois;
- o primeiro keepalive de init é `FB_TYPE_PLAYER_LED`, para não começar com `SET_RUMBLE`.

**Motivo:** o 8BitDo não vibra com `SET_CONF`; é preciso alternar corretamente entre `SET_CONF` e `SET_RUMBLE`.

**Arquivos:** `main/adapter/wireless/sw.c`, `main/bluetooth/hidp/sw.c`.  
**Validação:** compilação; **teste com 8BitDo/Switch pendente**.

### 4.4 Blink e pulso de conexão: timers auto-destrutivos

**v0.5.1:** o blink e o pulso usavam `esp_timer`; o callback chamava `esp_timer_delete()` no próprio timer. Em `esp_timer`, deletar de dentro do callback retorna `ESP_ERR_INVALID_STATE`; o timer podia continuar ativo, duplicar frames ou vazar. Em desconexão, o timer podia escrever em `bt_data` já limpo.

**v0.6:**

- o blink e o pulso são máquinas de estado (`active`, `tick_cnt`) acionadas por `bt_hid_ps_tick()` na `bt_fb_task` a cada ~10 ms;
- o blink usa 4 ticks de 10 ms = 40 ms por frame; 50 frames = 2 s por respiração;
- o pulso usa 15 ticks = 150 ms;
- `ps4_blink_stop()` apenas zera o estado;
- o pulso salva os motores antes e restaura **somente se** ainda estiverem com os valores do pulso (`0xFF/0x00`), para não sobrescrever o jogo.

**Motivo:** cancelamento seguro em desconexão, sem timer órfão e sem bloquear o caminho de feedback.

**Arquivos:** `main/bluetooth/hidp/ps.c`, `main/bluetooth/host.c`.  
**Validação:** compilação; **teste de conexão/desconexão rápida pendente**.

### 4.5 Memory card: perda de flush

**v0.5.1:** `mc_write()` marcava o bloco sujo e enfileirava `FB_TYPE_MEM_WRITE`. A fila de feedback é *best-effort* (spin curto); se o evento fosse descartado, o bloco continuava sujo, mas o timer de flush não era reiniciado. O save podia não ir para o SPIFFS.

**v0.6:** o `bt_fb_task()` lê `mc_get_state()` e, quando aparece um novo estado sujo sem evento, chama `mc_storage_update()`. O `adapter_q_fb()` documenta que o descarte é recuperável pelo sweep. O `manager.c` também evita trocar o estado das portas enquanto há bloco sujo.

**Importante:** o cartão virtual está **desligado no build padrão** (seção 5.7). O fix continua no código para o build standalone opcional.

**Arquivos:** `main/bluetooth/host.c`, `main/adapter/adapter.c`, `main/adapter/memory_card.c`, `main/system/manager.c`.  
**Validação:** compilação; **teste de save no build com cartão pendente**.

### 4.6 Fila TX: espera infinita e risco de deadlock

**v0.5.1:** `bt_host_txq_add()` usava `portMAX_DELAY`. Os callers já seguravam `bt_tx_mutex`; se o ring buffer estivesse cheio, a tarefa ficava bloqueada esperando o BTC processar o send-complete, que precisava do mesmo lock. Deadlock possível.

**v0.6:** espera limitada a **10 ms**. Se a fila estiver cheia, o pacote é descartado, o contador `txq_full` incrementa e o próximo feedback periódico renova o estado. HID é periódico; o descarte é preferível ao deadlock.

**Motivo:** robustez sob carga; não aumentar a espera sem medição.

**Arquivos:** `main/bluetooth/host.c`.  
**Validação:** compilação; **teste de carga/extress pendente**.

### 4.7 Concorrência: locks nos caminhos HCI, SDP, RX, feedback e reset

**v0.5.1:** vários caminhos montavam/enviavam pacotes sem o mesmo lock; o reset do dispositivo podia limpar `bt_data` enquanto o feedback ou o blink o lia.

**v0.6:**

- `bt_hci_start_inquiry()`, `bt_hci_stop_inquiry()` e `bt_hci_disconnect()` entram no lock;
- o `bt_sdp_parser()` e o `bt_hid_init()` chamado por ele entram no lock;
- os handlers de ACL/EVT no RX entram no lock;
- a `bt_fb_task` mantém o lock enquanto lê `INIT_DONE`, chama `bt_hid_ps_tick()` e o feedback;
- `bt_host_reset_dev()` entra no lock antes de `ps4_blink_stop()`/`adapter_init_buffer()`/`memset`;
- o PS3 e o Switch também checam `ids.type` e o timer dentro do lock.

**Motivo:** evitar use-after-free/estado inconsistente em desconexão, troca de controle e init concorrente.

**Arquivos:** `hci.c`, `host.c`, `ps3.c`, `sw.c`, `ps.c`.  
**Validação:** compilação; **teste de reconexão/desconexão sob carga pendente**.

### 4.8 Timers de init do PS3/Switch/DS4

**v0.5.1:** o callback do timer de init chamava `esp_timer_delete()` sem checar `timer_hdl`, sem checar se o dispositivo ainda era do mesmo tipo e sem o lock do reset.

**v0.6:** o callback:

- entra no `bt_host_tx_lock()`;
- verifica `ids.type` (PS3/PS/PS5/Switch);
- só deleta o timer se `timer_hdl` existir;
- publica `BT_DEV_HID_INIT_DONE` ao final dos pacotes de init;
- sai do lock.

**Motivo:** desconexão durante o init não pode limpar o dispositivo no meio do frame nem deixar timer órfão.

**Arquivos:** `ps3.c`, `sw.c`, `ps.c`.  
**Validação:** compilação; **teste de reconexão em <1 s pendente** (risco residual 8.7).

### 4.9 DualSense: `INIT_DONE` depois dos pacotes de init

**v0.5.1:** `BT_DEV_HID_INIT_DONE` era publicado antes de enfileirar todos os pacotes de init; a tarefa de feedback podia intercalar um report de jogo com o init.

**v0.6:** o `INIT_DONE` é publicado **depois** dos pacotes de init; o DS5 também usa `conf1 = 0x14`, `valid_flag2 = 0x02`, `lightbar_setup = 0x02` e `led_brightness` no init e no feedback.

**Motivo:** ordem determinística do init; LEDs/barra corretos desde o primeiro frame.

**Arquivos:** `ps.c`, `ps.h`, `wireless/ps.c`.  
**Validação:** compilação; **teste visual no DS5 pendente**.

### 4.10 Clone DS4: envio de `0x05` apenas no canal de interrupção

**v0.5.1/v0.6:** o `0x05` de clone é enviado no canal de interrupção, nunca no canal de controle, para evitar flood/handshake. A v0.6 mantém essa decisão e adiciona o lock/buffer dedicado no caminho do clone.

**Motivo:** clones reagem mal a `SET_REPORT` no canal de controle; o risco residual de clone `0x05` continua documentado (seção 8.2 da análise).

**Arquivos:** `ps.c`.  
**Validação:** **teste com clones reais pendente**.

---

## 5. Correções específicas de PS1/PS2

### 5.1 CCOUNT entre núcleos

**v0.5.1:** `packet_end()` (ISR bare-metal, core 1) gravava `xthal_get_ccount()`; `ps_spi_poll()` (core 0) comparava com o mesmo registrador. CCOUNT é por núcleo; o timeout de 3 s podia disparar cedo/tarde ou nunca.

**v0.6:**

- a ISR apenas incrementa `pkt_cnt` e publica `spi_active` (dois stores, sem timer);
- o poll no core 0 lê o CCOUNT **do core 0**, detecta a mudança de `pkt_cnt` e carimba `last_spi_ccnt`;
- `ps_idle_reset_ccnt = 3 * esp_clk_cpu_freq()`;
- a diferença de 32 bits cobre o wrap de ~17,9 s a 240 MHz;
- não há `esp_timer_get_time()` na ISR.

**Motivo:** remover a comparação inválida entre núcleos e deixar o caminho crítico mais leve.

**Arquivos:** `main/wired/ps_spi.c`.  
**Validação:** compilação; **teste de timeout/reset no PS1 pendente**.

### 5.2 Multitap e índice de saída (`mt_first_port`)

**v0.5.1:** `ps_spi_is_analog()` e o reset usavam `port->id` (número da porta SPI) como se fosse o índice de saída. Com multitap na porta 2, ou dual, o slot podia ser o errado; o LED/modo analógico do jogador 3/4 não correspondia.

**v0.6:** usa `port->mt_first_port + slot`; o `mt_first_port` é o índice real de saída (0, 1 ou 4 quando a outra porta tem multitap). O reset também trata `0x79` e `0xF3`, não só `0x73`.

**Motivo:** mapeamento correto de jogadores 3/4 e do modo analógico em multitap.

**Arquivos:** `ps_spi.c`.  
**Validação:** compilação; **teste de multitap nas duas portas pendente**.

### 5.3 Reset do console: IDs `0x73`, `0x79`, `0xF3`

**v0.5.1:** só `0x73` era resetado para `0x41` no timeout de 3 s.

**v0.6:** `0x73`, `0x79` e `0xF3` voltam a `0x41`, com `rumble`/LED resetados e `mt_first_port` correto.

**Motivo:** cobrir todos os IDs analógicos usados pelo PS1/PS2 e evitar modo analógico “preso” após reset.

**Arquivos:** `ps_spi.c`.  
**Validação:** compilação; **teste de reset físico pendente**.

### 5.4 Botão analógico: atomicidade (preservada)

**v0.5.1:** já tinha `ps_analog_busy` (`atomic_cas`) em `ps_analog_btn_hdlr()`, protegendo a alternância entre o poll e a ISR.

**v0.6:** mantém a mesma trava e a lógica; **não é uma mudança da v0.6**.

**Motivo:** evitar estado inconsistente entre `dev_id`, `pend_dev_id` e o buffer de resposta; a v0.5.1 já resolvia isso.

**Arquivos:** `ps_spi.c` (sem alteração funcional).  
**Validação:** compilação; **teste do botão PS/Home pendente**.

### 5.5 Build PS1-only

**v0.5.1:** compilava todos os consoles wired; a IRAM ficava em **99,0%** (1.309 B livres).

**v0.6:** quando `CONFIG_BLUERETRO_SYSTEM_PSX_PS2=y`:

- o `main/CMakeLists.txt` remove os adaptadores/drivers wired não-PlayStation (NES, SNES, N64, GC, DC, Saturn, PCE, 3DO, Jaguar, CD-i, PC-FX, JVS, paralelo, SEA, Wii-EXT);
- as tabelas de dispatch em `wired.c`/`wired_bare.c` mantêm o índice `WIRED_MAX`, mas os slots removidos viram `NULL` via `WIRED_OTHER()`;
- `wired_rtos_init()` fica vazio (paralelo/SEA não compilados);
- `BR_PS1_ONLY=1` é definido.

**Motivo:** liberar IRAM/flash para o caminho crítico e futuras funções, sem remover os controles wireless nem o PS2 compartilhado.

**Arquivos:** `main/CMakeLists.txt`, `wired.c`, `wired_bare.c`, `wired_rtos.c`.  
**Validação:** build completo e PS1-only comparados; **build universal não recompilado nesta rodada**.

### 5.6 PS2: mantido

**v0.5.1/v0.6:** `ps.c`/`ps_spi.c` são compartilhados; o código detecta `DEV_PS2_DS2`/`DEV_PS2_MULTITAP`.

**Decisão:** manter. O controle PS2 tende a funcionar com a mesma fiação, mas não foi homologado. O **memory card PS2 de 8 MB** não está implementado neste firmware; o MC está fora do escopo desta versão; o foco é o controle. Remover o PS2 economizaria poucos KB e mexeria na detecção compartilhada, com risco de regressão no PS1. O build já está na safe zone.

**Arquivos:** `main/CMakeLists.txt` (não remove PS2), `ps_spi.c`, `ps.c`.  
**Validação:** **PS2 não testado**.

### 5.7 Memory card: desligado no ESP32 principal

**v0.5.1:** `memory_card.c` emulava um cartão PS1 de **128 KB** no SPIFFS; o `mc_init_mem()` alocava 128 KB de heap DMA no boot. O `BR_PS1_MEMORY_CARD` não existia.

**v0.6:** o cartão virtual fica **desligado por padrão**:

- `BR_PS1_MEMORY_CARD` default **n**;
- `configs/hw1/playstation` tem `CONFIG_BR_PS1_MEMORY_CARD=n`;
- `memory_card.c` compila stubs (`mc_get_ready()` retorna `false`, sem alocar os 128 KB);
- o `ps_spi` não habilita `ACC_MEM` e o console não vê o cartão virtual;
- o MC está fora do escopo desta versão; o foco é o controle;
- o build standalone opcional pode ligar `=y`, pagando 128 KB de heap e ~1,3 KB de flash.

**Motivo:** manter o foco no controle, liberar **128 KB de heap DMA** e ~1,3 KB de flash para o stack BT e o serviço de configuração BLE. O custo de heap era o maior impacto real da v0.5.1.

**Arquivos:** `main/Kconfig.projbuild`, `configs/hw1/playstation`, `main/adapter/memory_card.c`, `main/bluetooth/host.c`, `main/wired/ps_spi.c`.  
**Validação:** build padrão 550.640 B; build opcional com cartão compila; **MC fora do escopo**.

---

## 6. Recursos novos

### 6.1 Menu Kconfig “PS1 Custom Edition”

**v0.5.1:** comportamento fixo no código; qualquer variante exigia editar `ps.c`/`ps_spi.c`.

**v0.6:** menu em `main/Kconfig.projbuild`:

| Opção | Padrão | Efeito |
|---|---|---|
| `BR_PS1_BLINK` | `y` | Blink azul N piscadas no DS4/clone. |
| `BR_PS1_CONN_RUMBLE` | `y` | Pulso de 150 ms. |
| `BR_PS1_LED_ANALOG_COLOR` | `0x00006D` | Cor da barra no analógico. |
| `BR_PS1_LED_DIGITAL_COLOR` | `0x101010` | Cor da barra no digital. |
| `BR_PS1_NIGHT_MODE` | `n` | Metade do brilho; DS5 `led_brightness=0x01`. |
| `BR_PS1_MEMORY_CARD` | `n` | Cartão virtual; **desligado** (fora do escopo). |
| `BR_PS1_BATTERY_IND` | `n` | Pulso âmbar com bateria ≤20%. |
| `BR_PS1_LATENCY_STATS` | `n` | Estatísticas no serial. |
| `BR_PS1_LATENCY_PROBE` | `n` | GPIO de probe para osciloscópio. |
| `BR_PS1_LATENCY_PROBE_GPIO` | `26` | Pino do probe. |

**Motivo:** permitir variantes de homologação/produção sem editar código, mantendo os padrões atuais.

**Arquivos:** `main/Kconfig.projbuild`, `ps.c`, `ps.h`, `wireless/ps.c`, `memory_card.c`, `host.c`, `ps_spi.c`.  
**Validação:** builds padrão, sem cartão, com cartão e com os caminhos condicionais ligados compilam.

### 6.2 Night mode

**v0.5.1:** sem opção.

**v0.6:** `BR_PS1_NIGHT_MODE` reduz as cores para metade, o blink para 64/255 e o DS5 para `led_brightness=0x01`. Padrão **n**.

**Motivo:** uso noturno sem alterar o padrão de fábrica.

**Arquivos:** `ps.c`, `ps.h`, `wireless/ps.c`.  
**Validação:** compilação; **teste visual pendente**.

### 6.3 Bateria baixa no DS4/DS5

**v0.5.1:** sem indicação.

**v0.6:** `BR_PS1_BATTERY_IND` (padrão **n**):

- DS4: byte `data[30]`; DS5: `data[53]`;
- nível ≤ 2/10 e sem carregamento;
- pulso âmbar de 150 ms a cada 30 s;
- não interfere no blink de conexão; salva/restaura a cor atual.

**Motivo:** aviso visual sem alterar a barra vermelha/cinza no uso normal. Fica desligado porque os offsets e o bit de carga precisam de confirmação no hardware.

**Arquivos:** `ps.c`.  
**Validação:** compilação; **teste com DS4/DS5 reais pendente**.

### 6.4 Estatísticas e probe de latência

**v0.5.1:** sem instrumentação específica; o teste de timing existente era de BT.

**v0.6:**

- `BR_PS1_LATENCY_STATS`: a cada 10 s imprime frames SPI, idle resets, drops da fila TX e a maior espera do `bt_host_tx_lock`;
- `BR_PS1_LATENCY_PROBE`: GPIO baixo em `bt_host_bridge()` (report HID recebido) e alto no fim do `packet_end()` (frame SPI), para medir o caminho BT → PS1 no osciloscópio;
- o probe usa um pino livre escolhido no Kconfig, **não** o barramento PS1; sem ele, nenhum fio extra.

**Motivo:** permitir medir a latência real em vez de afirmar “0 ms”. Não adiciona custo no caminho crítico quando desligado.

**Arquivos:** `ps_spi.c`, `host.c`, `host.h`.  
**Validação:** compilação; **medição com osciloscópio pendente**.

### 6.5 Cartão virtual opcional

O código do cartão e o fix de flush continuam disponíveis para um build standalone (`BR_PS1_MEMORY_CARD=y`). Não é o padrão porque o MC está fora do escopo desta versão.

### 6.6 Interface HTML / BLE

**v0.5.1:** já existia o serviço de configuração BLE (`att_cfg.c`), usado pelo web app oficial via **Web Bluetooth**. O HTML não está neste repositório. O `main/tests/ws_srv.c` é um servidor WebSocket de **teste via Ethernet**, não a interface do usuário.

**v0.6:** não embute HTML nem Wi-Fi/HTTP. A página separada para as novas opções é **fase 2**:

1. o firmware guardaria as opções em runtime (NVS/config) e exporia leitura/escrita por um comando `BR_CFG_CMD` novo;
2. a página `webconfig/` usaria Web Bluetooth (Chrome/Edge; Safari/iOS não suporta);
3. o custo no ESP32 seria pequeno; o HTML é host-side;
4. a configuração deve ser feita fora do jogo (o BLE divide o rádio com os controles).

**Motivo do cartão desligado:** liberar 128 KB de heap para o stack BT/config que atende essa página. O HTML em si não consome flash/IRAM do ESP32.

---

## 7. Impacto de recursos e por que o build está na safe zone

| Recurso | v0.5.1 | v0.6 padrão | Observação |
|---|---:|---:|---|
| Flash da app | 605.616 B | **550.640 B** | -54.976 B; 47% livre na partição de 1 MB |
| IRAM | 129.763 B (1.309 livres) | **106.135 B (24.937 livres)** | -23.628 B |
| DRAM | 57.452 B | **46.524 B (78.056 livres)** | -10.928 B |
| Heap do cartão virtual | 128 KB | **0** | O MC está fora do escopo |
| Fio extra | — | **0 no build padrão** | O probe é opcional e usa pino livre |

O build PS1-only foi o que resolveu o aperto de IRAM (99,0% → 81,0%). O cartão virtual foi o que liberou heap. O PS2 não foi removido porque o ganho seria pequeno e o risco de regressão no PS1 seria real. O HTML é host-side; o heap liberado é para o firmware que o atende.

---

## 8. Documentação, build e empacotamento

| Arquivo | v0.5.1 | v0.6 | Motivo |
|---|---|---|---|
| `README.md` | Documentação antiga, algumas alegações não suportadas | Atualizado; Kconfig, PS2/MC, patch, pacote; remove “20 ms sincronizado a vídeo”, “0x43 pause”, watchdog sem fonte, CI inexistente | Deixar o documento compatível com o código e com o que foi medido. |
| `ANALISE-v0.6-deepseek.md` | Não existia | Auditoria completa, matriz, riscos, decisões | Rastreabilidade técnica. |
| `COMPARATIVO-v0.5.1-v0.6-deepseek.md` | Não existia | Este documento | Explicar antes/depois e motivos para os devs. |
| `PACOTE-PARA-DEVS.md` | Não existia | Hashes, variantes, gravação, pendências | Entrega reproduzível. |
| `build.sh` | Não existia | Copia `configs/hw1/playstation`, checa `idf.py`, popula liblfds | Evitar `sdkconfig` antigo de outro alvo. |
| `scripts/fetch_liblfds.sh` | Não existia | Popula o submódulo 7.1.1 | Reprodutibilidade. |
| `scripts/package-release.sh` | Não existia | Gera pacote limpo `.zip`/`.tar.gz` sem `build/` | Enviar aos devs sem 134 MB de objetos. |
| `patches/v0.6-deepseek.patch` + `README` | Não existia | Patch formal contra a v0.5.1 | Revisão/aplicação profissional. |
| `.gitmodules` | Não existia | Declara o submódulo liblfds | Build reproduzível. |
| `configs/hw1/playstation` | HW1 + PSX/PS2 | + `CONFIG_BR_PS1_MEMORY_CARD=n` | MC fora do escopo; 128 KB de heap livres. |

---

## 9. O que **não** foi alterado (e por quê)

- **`hotplug = 1`** — requisito do modo invisível.
- **Deadzone `0x40` / HF `0xFF`** — emulação 1:1.
- **Sem gate `acc_mode & ACC_RUMBLE` no `0x42`** — feedback do jogo independente do acessório.
- **Controles wireless** — são entradas, não consoles; removê-los quebraria DS4/DS5/Switch/PS3/Wii/8BitDo.
- **PS2 compartilhado** — código comum; o controle PS2 tende a funcionar, o MC PS2 de 8 MB não está implementado.
- **Clone `0x05` no canal de interrupção** — decisão de compatibilidade; risco residual documentado.
- **`bt_host_q_wait_pkt(60)`** — caminho de autenticação/pareamento.
- **Triggers adaptativos do DS5** — já existiam; não foram removidos; o foco é PS1.
- **LEDs internos do ESP32** — não foram alterados; ficam dentro do console.
- **Compatibilidade com o web app oficial** — o `att_cfg.c` não foi quebrado; a página nova seria separada.

---

## 10. Validação: o que foi feito e o que falta

### Feito

- build universal (baseline) e PS1-only com ESP-IDF v5.5.1;
- build padrão **sem cartão** (550.640 B) e build opcional **com cartão** (551.984 B);
- build com os caminhos condicionais (blink/rumble off, cartão off, night/bateria/stats/probe on): compila;
- pacote limpo descompactado e compilado do zero: **0x866f0** (550.640 B, 47% livre);
- patch formal aplicado com `git apply --check`/`git apply` na base v0.5.1;
- comparação `diff -rq` entre a cópia de trabalho e o pacote limpo (fonte/docs/patch idênticos, ignorando artefatos gerados);
- inspeção `nm` dos símbolos PS1 e ausência dos adaptadores removidos.

### Pendente (bloqueante para homologação)

- boot sem controle BT e duas portas com fio;
- DS4, clone DS4 e DS5 (barra, player LEDs, blink/pulso);
- Switch/8BitDo (LED + rumble simultâneos);
- multitap nas duas portas;
- memory card fora do escopo e o build opcional com cartão no ESP32 principal;
- reconexão rápida, reset físico, timeout de 3 s;
- bateria baixa (confirmar `data[30]`/`data[53]` e bit de carga);
- night mode;
- estatísticas e probe de latência com pino livre;
- medição da latência BT → frame SPI contra a v0.5.1 e contra o controle com fio;
- PS2 (controle e MC de 8 MB);
- página Web Bluetooth (fase 2).

**Sem esses testes, não declarar “0 ms”, “igual ao fio” ou homologação.**

---

## 11. Arquivos alterados (33 arquivos no patch)

| Arquivo | Tipo | Mudança principal | Motivo |
|---|---|---|---|
| `.gitmodules` | novo | Submódulo liblfds 7.1.1 | Build reproduzível. |
| `ANALISE-v0.6-deepseek.md` | novo | Auditoria completa | Rastreabilidade. |
| `COMPARATIVO-v0.5.1-v0.6-deepseek.md` | novo | Este documento | Explicar antes/depois. |
| `PACOTE-PARA-DEVS.md` | novo | Pacote/hashes | Entrega. |
| `README.md` | modificado | Kconfig, PS2/MC, patch, pacote | Documentação correta. |
| `build.sh` | novo | Config + build | Evitar config errada. |
| `configs/hw1/playstation` | modificado | `CONFIG_BR_PS1_MEMORY_CARD=n` | MC fora do escopo. |
| `main/CMakeLists.txt` | modificado | PS1-only; remove fontes não-PS1 | IRAM/flash. |
| `main/Kconfig.projbuild` | modificado | Menu PS1 Custom | Variantes sem editar código. |
| `main/adapter/adapter.c` | modificado | Comentário do FB/MC | Documentar recuperação do flush. |
| `main/adapter/memory_card.c` | modificado | Stubs quando MC off | 128 KB de heap. |
| `main/adapter/wired/ps.c` | modificado | Comentário do motor HF | Documentação; comportamento preservado. |
| `main/adapter/wired/wired.c` | modificado | `WIRED_OTHER()`/`NULL` | PS1-only. |
| `main/adapter/wireless/ps.c` | modificado | Flags/cores/brilho DS5 | Layout e Kconfig. |
| `main/adapter/wireless/sw.c` | modificado | Marcador em todo feedback | Switch/8BitDo. |
| `main/bluetooth/hci.c` | modificado | Locks inquiry/disconnect | Concorrência. |
| `main/bluetooth/hidp/generic.c` | modificado | Buffer HID dedicado | Corrida de buffer. |
| `main/bluetooth/hidp/hidp.c` | modificado | `bt_hid_pkt_tmp` | Corrida de buffer/CRC. |
| `main/bluetooth/hidp/hidp.h` | modificado | Declaração do buffer | Idem. |
| `main/bluetooth/hidp/ps.c` | modificado | DS5, blink/pulso, locks, bateria | Correções e Kconfig. |
| `main/bluetooth/hidp/ps.h` | modificado | Layout DS5/player LEDs/helpers | Flags corretos. |
| `main/bluetooth/hidp/ps3.c` | modificado | Lock/tipo/timer | Init seguro. |
| `main/bluetooth/hidp/sw.c` | modificado | Lock/tipo/marcador/2 fases | Switch/8BitDo. |
| `main/bluetooth/hidp/wii.c` | modificado | Buffer HID dedicado | Corrida de buffer. |
| `main/bluetooth/host.c` | modificado | Locks, sweep MC, txq 10 ms, stats/probe | Robustez/latência. |
| `main/bluetooth/host.h` | modificado | Getters de stats | Instrumentação. |
| `main/system/manager.c` | modificado | `err_led_set` sob HW2 | Build sem warning. |
| `main/wired/ps_spi.c` | modificado | CCOUNT/pkt_cnt, multitap, IDs, stats/probe | PS1 correto e mensurável. |
| `main/wired/wired_bare.c` | modificado | `WIRED_OTHER()`/`NULL` | PS1-only. |
| `main/wired/wired_rtos.c` | modificado | No-op PS1-only | Paralelo/SEA fora. |
| `patches/README.md` | novo | Instruções do patch | Aplicação. |
| `scripts/fetch_liblfds.sh` | novo | Popula submódulo | Build. |
| `scripts/package-release.sh` | novo | Pacote limpo | Entrega. |

> O `git show --stat` do patch é a fonte exata de linhas; o número pode variar se este documento for atualizado.

---

## 12. Como aplicar, compilar e gravar

### Aplicar o patch na v0.5.1

```bash
git am patches/v0.6-deepseek.patch
# ou
git apply --check patches/v0.6-deepseek.patch
git apply patches/v0.6-deepseek.patch
# ou, sem Git:
patch -p1 < patches/v0.6-deepseek.patch
```

O patch **não** inclui `build/`, `sdkconfig`, o submódulo `liblfds` nem binários. Rode `scripts/fetch_liblfds.sh` se a pasta do submódulo estiver vazia.

### Compilar

```bash
. ~/esp/esp-idf/export.sh
./build.sh
```

O `build.sh` copia `configs/hw1/playstation` (PS1-only + cartão off) e compila. O binário sai em `build/BlueRetro_hw1_playstation.bin` (**550.640 B**).

### Gravar

```bash
cd firmware
python -m esptool --chip esp32 -p /dev/ttyUSB0 -b 460800 \
  --before default_reset --after hard_reset write_flash @flash_args
```

O `flash_args` grava bootloader `0x1000`, partições `0x8000`, OTA `0xd000` e app `0x10000`.

---

## 13. Riscos e pontos abertos

1. **Sem homologação em hardware.** Nenhum teste real de console/controle foi executado.
2. **Bateria baixa:** offsets `data[30]`/`data[53]` e bit de carga precisam de confirmação; por isso está desligada.
3. **Probe de latência:** o pino escolhido precisa estar livre; não usar um pino do barramento PS1.
4. **Night mode:** falta validação visual.
5. **Cartão virtual:** desligado no padrão; o MC está fora do escopo e não faz parte da validação.
6. **PS2:** controle não homologado; MC PS2 de 8 MB não implementado neste firmware.
7. **Clone `0x05`:** risco residual no canal de interrupção.
8. **Timers de init:** reconexão em <1 s pode duplicar/pular init (seção 8.7 da análise).
9. **Fila TX:** se saturar por >10 ms, o pacote é descartado; o próximo feedback renova.
10. **HTML/BLE:** a página é fase 2; o serviço BLE existente não foi alterado; a configuração deve ser feita fora do jogo.
11. **Latência:** o caminho crítico não foi onerado, mas “0 ms”/“igual ao fio” **não** foi medido.
12. **Build universal:** o caminho PS1-only foi validado; o universal não foi recompilado nesta rodada.

---

## 14. Conclusão

A v0.6-deepseek mantém a intenção e o comportamento visível da v0.5.1, mas corrige os pontos que impediam a estabilidade e a reprodutibilidade: layout do DS5, concorrência de buffers, feedback do Switch/8BitDo, timers, flush do cartão, multitap, CCOUNT e fila TX.

A arquitetura também foi ajustada ao projeto: build PS1-only, cartão virtual fora do ESP32 principal (fora do escopo; o foco é o controle), 128 KB de heap livres, Kconfig para variantes e instrumentação de latência. O próximo passo é a matriz de hardware; até lá, o documento deve ser lido como **build validado, comportamento preservado, homologação pendente**.
