# BlueRetro — PlayStation 1 Custom Edition
## Análise técnica da v0.5.1 e correções da v0.6-deepseek

**Base analisada:** `BlueRetro-master` (v0.5.1, fork “PlayStation 1 Custom Edition” do BlueRetro de Jacques Gagnon)
**Build corrigido:** `v0.6-deepseek/`
**Público:** time de desenvolvimento / homologação
**Idioma:** português

---

## 1. Resumo executivo

A v0.5.1 contém **quatro personalizações intencionais e funcionalizadas** que não são defeitos e que a v0.6-deepseek **preserva integralmente**:

1. ESP32 invisível ao console quando não há controle Bluetooth conectado, liberando as duas portas para controles com fio.
2. Emulação 1:1 do comportamento do controle com fio original (deadzone de 0x40 no motor de baixa frequência e saturação 0xFF no motor de alta frequência), validada em **THPS2, Resident Evil 3 e Alien Resurrection**.
3. LED azul piscando **N vezes conforme a porta** (1 piscada para o controle 1, 2 para o controle 2 etc.) e **vibração simples de confirmação de conexão**.
4. LED vermelho quando o modo analógico está ativo (barra `#6D0000`) e cinza suave no modo digital (`#101010`).

Além disso, a v0.6-deepseek implementa o comportamento solicitado para o **DualSense (DS5)**:

- usa os **LEDs de identificação de player do próprio DS5** (`player_leds`);
- mantém a **barra de luz cinza na conexão** e **vermelha quando o analógico está ativo**, igual ao DS4;
- **não** usa o blink azul do DS4 no DS5 (a identificação do DS5 é feita pelos LEDs de player).

Os defeitos reais da v0.5.1 estão concentrados em **layout/flags do DualSense, concorrência do buffer HID/HCI, regressão do feedback do Switch/8BitDo, temporizadores auto-destrutivos, perda de flush do memory card, mapeamento de multitap e CCOUNT entre núcleos**. A v0.6-deepseek corrige esses pontos sem alterar as quatro personalizações acima.

> **Status do build:** o código corrigido foi **compilado com ESP-IDF v5.5.1** neste ambiente. O binário PS1 (`build/BlueRetro_hw1_playstation.bin`) tem **551.968 bytes** (`0x86c20`), com **47% livres** na partição de aplicação de 1 MB. A IRAM ficou com **24.785 bytes livres (18,9%)** e a DRAM com **77.920 bytes livres (62,6%)**. O build PS1-only exclui os sistemas wired não-PlayStation; os números completos e a comparação com o build que ainda incluía todos os sistemas estão na seção 7.3. **A validação em hardware ainda não foi executada** (seção 6).

---

## 2. Personalizações confirmadas (não são defeitos)

### 2.1 ESP32 invisível sem controle Bluetooth

**O que faz:** com `hotplug = 1` e o gate de `BT_DEV_HID_INIT_DONE` em `main/system/manager.c`, a porta só entra no `port_mask` depois que um controle Bluetooth completa a inicialização HID. Sem controle BT, o ESP32 não responde ao console e as duas portas ficam disponíveis para controles com fio.

**Por que é intencional:** é o comportamento desejado para uso misto (controles com fio + BT) e para não “sequestrar” as portas quando o receptor está ligado sem controle pareado.

**O que a v0.6-deepseek faz:** **não altera** `hotplug`, o gate do manager nem a inicialização das portas.

**Cuidado de validação:** conectar um controle com fio nas duas portas com o receptor ligado e sem controle BT; depois conectar um controle BT e verificar a transição sem reiniciar o console.

---

### 2.2 Emulação 1:1 do controle com fio (deadzone 0x40 / HF 0xFF)

**O que faz:** em `main/adapter/wired/ps.c`:

- motor de baixa frequência (LF): valores abaixo de `0x40` são tratados como repouso (`0x00`);
- motor de alta frequência (HF): o código testa o bit 0 do byte do jogo; os valores do protocolo (`0x01`/`0xFF`) saturam em `0xFF`;
- o gate `acc_mode & ACC_RUMBLE` foi removido do comando `0x42`, permitindo que o feedback do jogo chegue ao controle sem depender da configuração de acessórios.

**Por que é intencional:** reproduz o limiar eletromecânico do SCPH-1200 com fio (o motor de ferro não parte abaixo de 0x40) e a resposta binária do motor leve. Foi validado em **THPS2, RE3 e Alien Resurrection**.

**O que a v0.6-deepseek faz:** **não altera** deadzone, saturação, gate de `acc_mode` nem a lógica de `ps.c` com fio.

**Cuidado de validação:** repetir os três jogos e comparar lado a lado com um controle com fio na mesma porta.

---

### 2.3 LED azul N piscadas por porta + vibração de conexão

**O que faz (DS4 e clones):** em `main/bluetooth/hidp/ps.c`, `ps4_start_player_blink()` inicia um blink azul de 2 s por “piscada”, com `total_breaths = out_idx + 1` (controle 1 = 1 piscada, controle 2 = 2 piscadas etc.). Em paralelo, `ps_start_player_rumble()` dá um pulso de 150 ms no motor leve.

**Por que é intencional:** é a camada de identificação visual e tátil da conexão. Não é um efeito do jogo nem um bug de LED.

**O que a v0.6-deepseek faz:**

- mantém o blink e o pulso;
- **remove os `esp_timer` auto-destrutivos** e passa o blink/pulso para `bt_hid_ps_tick()`, chamado a cada ~10 ms pela `bt_fb_task`;
- salva e restaura os motores no pulso de conexão, para não “engolir” um rumble do jogo que já estivesse ativo. A checagem `HF == 0xFF && LF == 0` é heurística: se o jogo pedir **exatamente** essa combinação durante os 150 ms, o stop restaura os valores anteriores; o próximo feedback do jogo corrige em ≤20 ms;
- publica `BT_DEV_HID_INIT_DONE` **depois** dos pacotes de inicialização, para a tarefa de feedback não intercalar um relatório de jogo no meio do init.

**DS5:** não usa o blink azul; usa os LEDs de player do próprio controle (ver 2.5).

**Cuidado de validação:** conectar o controle 1 e o controle 2 e contar as piscadas; conectar durante um rumble contínuo e verificar se o pulso não corta o rumble do jogo.

---

### 2.4 LED vermelho no analógico / cinza no digital

**O que faz:** a barra de luz usa:

- analógico ativo: `0x00006D` (vermelho SCPH-1200 com ~15% de atenuação);
- digital: `0x101010` (cinza suave).

**Por que é intencional:** identifica o modo analógico sem ofuscar; é a mesma linguagem visual do LED do SCPH-1200.

**O que a v0.6-deepseek faz:** preserva os valores e corrige a **detecção de modo** para multitap e para o DS5 (ver 4.6). O DS5 passa a usar a mesma barra cinza/vermelha.

**Cuidado de validação:** alternar o botão analógico nos dois controles, com e sem multitap, e conferir LED do receptor + barra do DS4/DS5.

---

### 2.5 DualSense: LEDs de player + barra cinza/vermelha (novo na v0.6-deepseek)

**Comportamento solicitado e implementado:**

- o DS5 usa os **LEDs de identificação de player do próprio controle** (`player_leds`), com os padrões de 1 a 5 LEDs;
- a barra de luz fica **cinza na conexão** (`0x101010`) e **vermelha no analógico** (`0x00006D`);
- **não** há blink azul no DS5;
- o pulso de vibração de conexão continua.

**Layout correto do report USB 0x02 do DualSense (77 bytes):**

| Offset | Campo | Valor na v0.6-deepseek |
|---:|---|---|
| 0 | `conf0` (report ID) | `0x02` |
| 1 | `cmd` (`valid_flag0`) | `0x03` (motores) |
| 2 | `conf1` (`valid_flag1`) | `0x14` (lightbar `0x04` + player indicator `0x10`) |
| 39 | `valid_flag2` | `0x02` (lightbar setup válido) |
| 40–41 | `reserved3[2]` | `0x00` |
| 42 | `lightbar_setup` | `0x02` |
| 43 | `led_brightness` | `0x02` |
| 44 | `player_leds` | máscara do jogador |
| 45–47 | RGB da barra | `0x6D0000` (analógico) / `0x101010` (digital) |
| 73–76 | CRC32 | calculado |

**Padrões de player:**

| Jogador | Máscara | LEDs |
|---:|---:|---|
| 1 | `0x04` | `. . * . .` |
| 2 | `0x0A` | `. * . * .` |
| 3 | `0x15` | `* . * . *` |
| 4 | `0x1B` | `* * . * *` |
| 5 | `0x1F` | `* * * * *` |
| 6–7 | `0x1F` | todos (o DS5 só tem 5 LEDs; o firmware limita ao padrão máximo) |

---

## 3. Defeitos reais da v0.5.1 corrigidos na v0.6-deepseek

### 3.1 P1 — Layout e flags do DualSense

**Problema:** o struct `bt_hidp_ps5_set_conf` em `main/bluetooth/hidp/ps.h` tinha um byte extra (`use_accurate_rumble`) antes de `valid_flag2`. Isso deslocava:

- `valid_flag2` para o offset 40 (em vez de 39);
- `lightbar_setup` para o offset 43 (em vez de 42);
- e **não existia** `led_brightness` no offset 43.

Além disso, o código escrevia `valid_flag2 = 0x14` e `conf1 = 0x04`, o que não habilitava o player indicator. Resultado: a barra e os LEDs de player do DS5 não eram configurados corretamente.

**Correção:**

- layout de 77 bytes com `valid_flag2` em 39, `reserved3[2]` em 40–41, `lightbar_setup` em 42, `led_brightness` em 43, `player_leds` em 44, RGB em 45–47 e CRC em 73–76;
- `conf1 = 0x14`, `valid_flag2 = 0x02`, `lightbar_setup = 0x02`, `led_brightness = 0x02` no init, no `ps5_clear_led`/`ps5_set_led` e em **todo** feedback (`ps5_fb_from_generic`);
- máscara de player de 5 entradas com clamp para 6–7.

**Arquivos:** `main/bluetooth/hidp/ps.h`, `main/bluetooth/hidp/ps.c`, `main/adapter/wireless/ps.c`.

**Verificação:** um programa de checagem de offsets confirma `sizeof = 77`, `valid_flag2 = 39`, `lightbar_setup = 42`, `led_brightness = 43`, `player_leds = 44`, `rgb = 45`, `crc = 73`.

---

### 3.2 P1 — Concorrência do buffer HID/HCI (`bt_hci_pkt_tmp`)

**Problema:** o mesmo `bt_hci_pkt_tmp` era usado por:

- comandos HCI e respostas ATT/L2CAP/SDP/SMP na tarefa de RX do Bluetooth;
- relatórios HID de feedback na `bt_fb_task`, nos timers de blink/pulso e no clone 0x05 no handler HID.

O mutex `bt_tx_mutex` da v0.5.1 cobria o `bt_hid_cmd` e os wrappers PS4/PS5, mas a montagem do relatório HID (cabeçalho L2CAP/HIDP + CRC) acontecia **fora** do lock no clone 0x05 e no Switch, e o `bt_hci_cmd`/ATT não usavam o lock. Isso podia gerar CRC inválido, pacotes truncados ou relatórios trocados.

**Correção:**

- **buffer HID dedicado** `bt_hid_pkt_tmp` (definido em `main/bluetooth/hidp/hidp.c`, declarado em `hidp.h`), separado do `bt_hci_pkt_tmp` de HCI/ATT. Todos os arquivos `main/bluetooth/hidp/*.c` usam o novo buffer;
- o `bt_hid_cmd()` continua com o mutex recursivo, e agora o clone 0x05 (`bt_hid_cmd_ps4_clone_set_conf`) também entra/sai do lock, inclusive quando chamado diretamente pelo `bt_hid_ps_hdlr`;
- o `bt_hid_cmd_sw_set_conf` também entra/sai do lock;
- **serialização grossa de HCI/ATT** nos limites de tarefa:
  - `bt_host_rx_pkt()` executa `bt_host_acl_hdlr()` e `bt_hci_evt_hdlr()` sob `bt_host_tx_lock()`;
  - o parser SDP da `bt_host_task` executa sob o mesmo lock;
  - `bt_hci_start_inquiry()`, `bt_hci_stop_inquiry()` e `bt_hci_disconnect()` também usam o lock (são chamados pelo manager e pelo RX);
  - no `bt_fb_task`, o `adapter_bridge_fb()` (que escreve `base.output`) também roda sob o lock, para não colidir com o `memset` de `bt_host_reset_dev()`;
- **espera limitada na fila de TX:** `bt_host_txq_add()` passou de `portMAX_DELAY` para **10 ms**. Com o lock grosso de RX, uma fila cheia + espera infinita poderia bloquear o produtor e, ao mesmo tempo, o BTC task que precisa processar o *send-complete* e liberar `BT_CTRL_READY` — deadlock. Se a fila continuar cheia, o pacote é descartado; o feedback HID é periódico e o próximo relatório recompõe o estado.

**Resultado:** o caminho de feedback HID não compartilha mais o buffer com HCI/ATT, e os construtores de HCI/ATT que rodam em tarefas diferentes ficam serializados nos pontos conhecidos. Ainda existe um risco residual em caminhos raros de comandos HCI disparados diretamente por tarefas externas (ver 8.3), mas o caminho que causava CRC corrompido no DS4/DS5 está fechado.

**Arquivos:** `main/bluetooth/hidp/hidp.h`, `hidp.c`, `ps.c`, `sw.c`, `generic.c`, `ps3.c`, `wii.c`; `main/bluetooth/host.c`, `main/bluetooth/hci.c`.

---

### 3.3 P1 — Regressão do `output[127]` no Switch/8BitDo

**Problema:** o marcador `output[127]` era gravado **somente** em eventos de rumble. Como o buffer zerado já começa com `FB_TYPE_RUMBLE == 0`, todo feedback de LED virava `SET_RUMBLE` quando `BT_DEV_RUMBLE_ON` estava setado. O LED do Switch/8BitDo era descartado, e o `bt_hid_cmd_sw_set_conf` ainda lia `[127]` de um `struct bt_hidp_sw_conf` local de 48 bytes no caminho de init (leitura fora dos limites).

**Correção:**

- `sw_fb_from_generic()` grava `output[127] = fb_data->type` em **todo** feedback;
- `bt_hid_cmd_sw_set_conf()` só lê o marcador quando `report == bt_data->base.output` (buffer persistente de 128 bytes); no caminho de init usa `SET_CONF`;
- se um evento de LED chega com rumble ativo, envia primeiro o `SET_CONF` (LED) e em seguida um `SET_RUMBLE` com os bytes de LRA já presentes no buffer, rearmando o rumble e marcando `output[127] = FB_TYPE_RUMBLE`;
- o init do Switch marca `output[127] = FB_TYPE_PLAYER_LED`, para o primeiro keepalive ser `SET_CONF` (LED), não `SET_RUMBLE`.

**Arquivos:** `main/adapter/wireless/sw.c`, `main/bluetooth/hidp/sw.c`.

---

### 3.4 P1 — Temporizadores auto-destrutivos (blink e pulso de conexão)

**Problema:** `ps4_blink_timer_cb()` e `ps_conn_rumble_timer_cb()` faziam `esp_timer_stop()` + `esp_timer_delete()` **de dentro do próprio callback**. No ESP-IDF, `esp_timer_delete()` retorna `ESP_ERR_INVALID_STATE` enquanto o callback está em execução; o timer podia continuar ativo, e o contexto era liberado, gerando use-after-free em desconexões/quedas.

**Correção:**

- blink e pulso de conexão **não usam mais `esp_timer`**; são máquinas de estado acionadas por `bt_hid_ps_tick()` a cada ~10 ms na `bt_fb_task`:
  - blink: 4 ticks = 40 ms por frame; 50 frames = 2 s por “piscada”;
  - pulso: 15 ticks = 150 ms;
- `ps4_blink_stop()` apenas zera `active`/`device`/`tick_cnt`;
- o timer de init de 1 s do DS4 continua sendo um `esp_timer` one-shot; o `esp_timer_delete()` tem guarda `if (device->timer_hdl)` (evita delete duplo, mas **não** elimina o risco de reconexão rápida descrito em 8.7);
- o callback de init (`bt_hid_ps5_init_callback`) agora entra no `bt_host_tx_lock`, checa `device->ids.type == BT_PS` e segura o lock durante todo o init; assim um disconnect não faz `memset(device)` no meio do callback;
- os callbacks de init do PS3 e do Switch (`bt_hid_ps3_init_callback`, `bt_hid_sw_init_callback`) recebem o mesmo tratamento, com checagem de `BT_PS3`/`BT_SW`;
- `bt_hid_ps_tick()` também roda sob o mesmo lock da `bt_host_reset_dev()`, eliminando o *use-after-free* de um blink/pulso em andamento durante a desconexão;
- `INIT_DONE` é publicado **depois** dos pacotes de init e **antes** de iniciar blink/pulso.

**Arquivos:** `main/bluetooth/hidp/ps.c`, `main/bluetooth/host.c` (chamada de `bt_hid_ps_tick`).

---

### 3.5 P1 — Perda de flush do memory card (`FB_TYPE_MEM_WRITE`)

**Problema:** `adapter_q_fb()` é “best effort”: tenta o spinlock 300 vezes e, se não conseguir, **descarta** o evento. O `mc_write()` enfileira `FB_TYPE_MEM_WRITE` por esse caminho; um descarte podia perder o flush de um save (o bloco ficava sujo em `mc_block_state`, mas ninguém iniciava o timer de gravação).

**Correção:**

- a `bt_fb_task` agora varre `mc_get_state()` a cada ciclo; quando o estado sujo aparece **sem** um evento correspondente (`mc_state_last == 0`), chama `mc_storage_update()`;
- o debounce de 1 s é preservado; o flush não é mais perdido se o evento for descartado.

**Arquivos:** `main/bluetooth/host.c`, `main/adapter/adapter.c` (comentário).

---

### 3.6 P2 — Mapeamento de multitap e reset de modo

**Problema:**

- o reset por inatividade usava `port->id + (MT ? j : 0)` para o `wired_id`, o que apontava para os slots errados no multitap (os slots são `mt_first_port + j`);
- `ps_spi_is_analog()` usava `port->id` no caminho direto; com multitap na outra porta, o índice direto pode ser 4 (não 1), então o LED/barra não refletia o modo;
- o reset só tratava `dev_id == 0x73`; dispositivos `0x79` (analógico com descritor estendido) e `0xF3` (modo config) ficavam presos;
- `ps_spi_is_analog()` não considerava `0x79`.

**Correção:**

- reset usa `port->mt_first_port + j`;
- `ps_spi_is_analog()` usa `port->mt_first_port` no caminho direto;
- reset trata `0x73`, `0x79` e `0xF3`;
- `ps_dev_id_is_analog()` considera `0x73` e `0x79`.

**Arquivos:** `main/wired/ps_spi.c`.

---

### 3.7 P2 — CCOUNT entre núcleos

**Problema:** `packet_end()` roda na ISR bare-metal do **core 1** e gravava `xthal_get_ccount()`; `ps_spi_poll()` roda na `sys_mgr_task` no **core 0** e comparava com o mesmo registrador. CCOUNT é **por núcleo** e não é comparável entre cores; o timeout de 3 s podia disparar ou nunca disparar, dependendo da deriva entre os contadores.

**Correção (sem adicionar latência ao caminho crítico):**

- `packet_end()` **não lê relógio nenhum**. A ISR apenas incrementa `pkt_cnt` e publica `spi_active = 1`, nessa ordem. O custo adicional no core 1 é um store de 32 bits por pacote SPI;
- `ps_spi_poll()` lê `xthal_get_ccount()` no **core 0**, compara `pkt_cnt` com `last_pkt_cnt` e, quando houve pacote novo, carimba `last_spi_ccnt` com o CCOUNT **do próprio core 0**;
- o timeout de inatividade compara `(uint32_t)(cur - last_spi_ccnt) < ps_idle_reset_ccnt`, com `ps_idle_reset_ccnt = 3 * esp_clk_cpu_freq()` calculado uma única vez em `ps_spi_init()`;
- o sinal cross-core é somente o contador `volatile uint32_t pkt_cnt` (32 bits alinhado, escrita única pela ISR, leitura pelo poll). Não há timestamp global, chamada de timer, divisão de 64 bits nem acesso a flash na ISR;
- o primeiro pacote é detectado naturalmente porque `pkt_cnt` muda de 0 para 1; não é mais necessário o caso especial `last == 0`.

**Por que isso preserva o “quase 0 de delay”:** o caminho com fio (SPI/ISR) continua sendo o mesmo; a ISR ficou **mais leve** do que a v0.5.1 (um contador em vez de `xthal_get_ccount()` + store) e não executa `esp_timer_get_time()`. O poll usa CCOUNT local do core 0, que é uma leitura de registrador. A diferença de 32 bits é válida para o timeout de 3 s porque o wrap do CCOUNT a 240 MHz ocorre a cada ~17,9 s.

**Observação:** se a frequência de CPU for alterada em runtime (não é o caso desta build), `ps_idle_reset_ccnt` precisa ser recalculado. **Validar em hardware** a estabilidade do timeout de 3 s e a ausência de reset falso no boot.

**Arquivos:** `main/wired/ps_spi.c`.

### 3.8 Kconfig “PS1 Custom Edition” e variantes (v0.6-deepseek)

A v0.6-deepseek adiciona um menu **Kconfig → PS1 Custom Edition** (`main/Kconfig.projbuild`) com os padrões atuais preservados:

| Opção | Padrão | Efeito |
|---|---|---|
| `BR_PS1_BLINK` | `y` | Blink azul N piscadas no DS4/clone; o DS5 não usa blink. |
| `BR_PS1_CONN_RUMBLE` | `y` | Pulso de 150 ms nos motores na conexão. |
| `BR_PS1_LED_ANALOG_COLOR` | `0x00006D` | Cor da barra no modo analógico. |
| `BR_PS1_LED_DIGITAL_COLOR` | `0x101010` | Cor da barra no modo digital. |
| `BR_PS1_NIGHT_MODE` | `n` | Metade do brilho das cores e do blink; brilho menor no DS5. |
| `BR_PS1_MEMORY_CARD` | `n` | Memory card virtual PS1 de 128 KB. Desligado porque o foco desta versão é o controle e o MC está fora do escopo; libera 128 KB de heap DMA. |
| `BR_PS1_BATTERY_IND` | `n` | Pulso âmbar a cada 30 s com bateria ≤ 20% e sem carregador. |
| `BR_PS1_LATENCY_STATS` | `n` | Estatísticas a cada 10 s no console serial. |
| `BR_PS1_LATENCY_PROBE` | `n` | GPIO de probe para osciloscópio; usa um pino livre, não o barramento PS1. |
| `BR_PS1_LATENCY_PROBE_GPIO` | `26` | Pino do probe (só existe quando o probe está ligado). |

**Builds testados nesta rodada:**

- **padrão (sem cartão virtual):** `550.640 B` (`0x866f0`), 47% de flash livre; IRAM 106.135 B (24.937 livres); DRAM 46.524 B (78.056 livres); **128 KB de heap DMA livres**;
- **opcional com cartão virtual:** `551.984 B` (`0x86c30`), 1.344 B maior; o cartão virtual custa 128 KB de heap no boot;
- **variante de teste com os caminhos condicionais:** `551.344 B` (`0x869b0`), com blink/rumble desligados, cartão desligado, night/bateria/stats/probe ligados. Ela prova que os `#if` compilam; **não** é a variante entregue.

**Bateria baixa (DS4/DS5):** o parse usa o byte de bateria do report PS4 (`data[30]`) e do PS5 (`data[53]`); o pulso âmbar só ocorre se o nível for ≤ 2/10 e o bit de carregamento estiver limpo, e é suspenso enquanto o blink de conexão está ativo. O código foi compilado, mas **os offsets e o bit de carga precisam ser confirmados em hardware**; por isso a opção vem desligada. Para ativar: `CONFIG_BR_PS1_BATTERY_IND=y` no `configs/hw1/playstation` (ou menuconfig) e reconstruir.

**Latência (stats/probe):** as estatísticas somam frames SPI, resets por idle, drops da fila TX e a maior espera do `bt_host_tx_lock`, impressas a cada 10 s. O probe opcional baixa um GPIO em `bt_host_bridge()` (report HID recebido) e sobe no fim do próximo `packet_end()` (frame SPI), permitindo medir o caminho BT → PS1 no osciloscópio. **Não usa nenhum pino do barramento PS1**; o pino é escolhido no Kconfig e fica desligado por padrão. Sem o probe, o custo é só o código das estatísticas (desligado por padrão).

**Night mode:** o modo noturno reduz as cores para metade e o blink para 64/255; o DS5 usa `led_brightness = 0x01`. Vem desligado; a página Web Bluetooth (fase futura) pode expor isso em runtime.

**Memory card virtual (o que é e por que está desligado):** o firmware **pode** emular um **memory card PS1 de 128 KB** dentro do SPIFFS; o `mc_write()` marca o bloco de 4 KB e o `mc_store_spread()` grava um bloco a cada 20 ms, com 1 s de debounce. Neste projeto, o foco é o controle; o MC está fora do escopo desta versão, então o cartão virtual do ESP32 principal fica **desligado por padrão** (`BR_PS1_MEMORY_CARD=n`): isso libera **128 KB de heap DMA** e mantém o foco no controle. O código do cartão e o fix de flush (`FB_TYPE_MEM_WRITE` + sweep de `mc_block_state`) continuam no repositório para um build **standalone** opcional; nesse caso, habilite `CONFIG_BR_PS1_MEMORY_CARD=y` e aceite o custo de 128 KB. O “pulso de gravação” não foi implementado; ele só faria sentido com esse cartão virtual habilitado.

### 3.9 PS2 compartilhado, memory card e a página de configuração

**PS2:** o adaptador `ps.c`/`ps_spi.c` é compartilhado e o código já detecta `DEV_PS2_DS2` e `DEV_PS2_MULTITAP`. O **controle** PS2 usa a mesma família de protocolo de 3 fios do PS1 e, com a mesma fiação, **tende a funcionar**; isso ainda não foi validado no console. O **memory card PS2 de 8 MB** é outro protocolo e **não está implementado** neste firmware; o MC está fora do escopo desta versão; o foco é o controle. Para jogos PS2 que exigem o cartão de 8 MB, use o cartão físico; o firmware principal não emula esse cartão.

**Vale remover o PS2?** Não. O controlador PS2 compartilha praticamente todo o `ps.c`/`ps_spi.c`; o corte economizaria poucos KB e mexeria na detecção compartilhada, com risco de regressão no PS1. O build já está na **safe zone** (47% de flash livre, 24.937 B de IRAM livre). O ganho real de memória já foi obtido desligando o **cartão virtual** (128 KB de heap), que agora está fora do escopo; o foco é o controle. Portanto: **PS2 fica**, cartão virtual fica **desligado** no build padrão e pode ser reativado só para um build standalone.

**Página de configuração via Bluetooth:** o firmware já tem o **serviço de configuração BLE** (`main/bluetooth/att_cfg.c`, `BR_ABI_VER 2`), que é o que o web app oficial do BlueRetro usa via **Web Bluetooth**; o HTML **não está neste repositório** (o `static/` contém apenas logos) e o `main/tests/ws_srv.c` é um **servidor WebSocket de teste via Ethernet**, não a interface do usuário. Não há Wi-Fi/HTTP no build de produção, e não vale adicionar: o rádio e a RAM são necessários ao Bluetooth dos controles.

É possível, sim, criar uma **página Web Bluetooth separada** para as opções novas, sem embarcar HTML no firmware:

1. o firmware guarda as opções em runtime (NVS/config) e expõe leitura/escrita por um comando `BR_CFG_CMD` novo;
2. a página HTML/JS conecta no serviço BLE existente, lê/escreve só os campos novos e não mexe no serviço oficial;
3. o frontend fica em `webconfig/` (arquivo separado, aberto no Chrome/Edge; Web Bluetooth não funciona no Safari/iOS);
4. o custo no firmware é pequeno (alguns campos + handler); o custo do HTML é **host-side**, não consome flash/IRAM do ESP32;
5. a configuração deve ser feita **fora do jogo**; a conexão BLE de configuração coexiste com os controles, mas divide o rádio e pode adicionar jitter.

Isso é uma **fase 2** e não foi implementado nesta rodada. O `att_cfg.c` atual continua intacto; a página oficial pode continuar sendo usada para o que ela já conhece (mapeamento, acessórios). O modo diagnóstico também pode virar um comando BLE na fase 2, em vez de um combo de botões.

**Por que o cartão virtual foi desligado agora:** o foco desta versão é o controle e o MC está fora do escopo; o ESP32 principal precisa de heap para o stack BT e o serviço de configuração BLE. Desligar o cartão virtual libera **128 KB de heap DMA** e ~1,3 KB de flash, mantendo o foco no controle.

### 3.10 Patch formal e relação com `dist/`

O patch formal está em `patches/v0.6-deepseek.patch` (formato `git format-patch`, base **v0.5.1**). Ele contém 33 arquivos, incluindo o Kconfig, o PS1-only, as correções, o `configs` com MC off e a documentação; o `git show --stat` do patch é a fonte exata de linhas. O `patches/README.md` explica como aplicar (`git am`, `git apply` ou `patch -p1`). O patch **não** inclui `build/`, `sdkconfig`, o submódulo `liblfds` nem os binários; o binário está em `firmware/` no pacote limpo.

A pasta `dist/` (fora de `v0.6-deepseek/`) contém o **pacote limpo** `BlueRetro-PS1-v0.6-deepseek/`, os arquivos `.zip`/`.tar.gz` e `SHA256SUMS`. O conteúdo **não é idêntico** à pasta `v0.6-deepseek/`: o pacote limpo tem a mesma fonte/documentação/patch, mas **não** tem `build/`, `sdkconfig`, o `.git` do `liblfds` nem o `scripts/package-release.sh` (ferramenta da cópia de trabalho); em troca, tem `firmware/` com os binários e o `LEIA-ME.txt`. Se você compactar a pasta `v0.6-deepseek/` inteira, terá a cópia de trabalho (com 134 MB de `build/`); para os devs, use o `.zip` do `dist/`.

---

## 4. O que a v0.6-deepseek **não** alterou (e por quê)

| Item | Decisão |
|---|---|
| `hotplug = 1` | Intencional; implementa o modo invisível sem controle BT. |
| Deadzone `0x40` / HF `0xFF` | Intencional; emulação 1:1 validada em THPS2/RE3/Alien. |
| Remoção do gate `acc_mode & ACC_RUMBLE` no `0x42` | Intencional; feedback do jogo não depende de `acc_mode`. |
| Blink azul N piscadas (DS4/clone) | Intencional; mantido, agora sem `esp_timer` auto-destrutivo. |
| Pulso de vibração de conexão | Intencional; mantido, agora com save/restore dos motores. |
| LED vermelho/cinza | Intencional; mantido. |
| `bt_host_q_wait_pkt(60)` | Mantido; é o caminho de autenticação/pareamento do fork. |
| Clone 0x05 no canal de interrupção | Mantido; decisão de design do fork. Precisa de teste com clones reais (ver 8.2). |
| `ADAPTER_RUMBLE_DBG` / testes | Mantidos; não afetam o build de produção. |

---

## 5. Fluxo de feedback depois da v0.6-deepseek

```
Jogo PS1
  └─ SPI (ISR core 1) → ps_spi.c → raw_fb (FB_TYPE_RUMBLE / STATUS_LED / PLAYER_LED)
       └─ adapter_q_fb() (spinlock; pode descartar)
            └─ bt_fb_task (core 0, 10 ms)
                 ├─ drena a fila
                 │    ├─ FB_TYPE_MEM_WRITE → mc_storage_update()
                 │    ├─ DS5: envia clear_led antes do set
                 │    └─ adapter_bridge_fb() → ps4_fb / ps5_fb (buffer de 128 B)
                 ├─ sweep de mc_block_state (recupera MEM_WRITE descartado)
                 ├─ bt_hid_ps_tick() → blink/pulso de conexão (sem esp_timer)
                 └─ bt_hid_feedback() → ps4/ps5/sw/... → bt_hid_cmd() → bt_hid_pkt_tmp → txq
                      (tudo sob bt_host_tx_lock; txq com espera limitada de 10 ms)
```

**Buffers:**

- `bt_hci_pkt_tmp`: HCI/ATT/L2CAP/SDP/SMP (RX/host/manager, serializado por `bt_host_tx_lock` nos limites de tarefa);
- `bt_hid_pkt_tmp`: relatórios HID de feedback (PS4, DS5, Switch, Wii, PS3, genérico, clone);
- `bt_data.base.output[128]`: buffer persistente por controle; o byte 127 é o marcador de tipo de feedback do Switch (não é enviado ao controle).

### 5.1 Latência: o que o firmware controla e o que não controla

O trajeto tem duas metades:

1. **Controle Bluetooth → ESP32:** rádio, stack BT e parser HID. Essa latência é do controle e do ar; o firmware **não a elimina**. DS4/DS5 em Bluetooth reportam tipicamente a ~250 Hz (4 ms por relatório), mas o intervalo real depende do controle, do ambiente RF e do stack.
2. **ESP32 → PS1:** o PS1 é o mestre do SPI; o `ps_spi.c` responde dentro do frame. É aqui que o firmware pode adicionar ou não latência.

No caminho PS1, a v0.6-deepseek:

- mantém a ISR `packet_end()` mínima: **um incremento de contador** e a flag `spi_active`, sem `esp_timer_get_time()`, sem alocação e sem acesso a flash;
- usa o CCOUNT do **core 0** no `ps_spi_poll()` para o timeout de 3 s, não no core 1;
- não adiciona nenhuma fila, lock ou espera no caminho ISR → `ps_spi`;
- preserva a emulação 1:1 (deadzone `0x40`, saturação `0xFF`), sem processamento extra de eixos.

O que pode atrasar o **input** e precisa de medição:

- a contenção do `bt_host_tx_lock` entre o RX (que processa o relatório) e a tarefa de feedback (que monta/enfileira HID); o lock cobre build + enqueue e, no caminho normal, o enqueue não bloqueia;
- `bt_host_txq_add()` espera no máximo 10 ms **somente** com a fila TX cheia; em operação normal é não bloqueante;
- o escalonamento da `sys_mgr_task` e o intervalo de polling do PS1, herdados da base.

**Instrumentação adicionada (opcional, desligada por padrão):** `CONFIG_BR_PS1_LATENCY_STATS` imprime a cada 10 s frames SPI, resets por idle, drops da fila TX e a maior espera do lock; `CONFIG_BR_PS1_LATENCY_PROBE` baixa um GPIO em `bt_host_bridge()` e sobe no fim do `packet_end()`, para o osciloscópio medir o caminho BT → frame SPI. O probe usa **um pino livre escolhido no Kconfig** (não o barramento PS1); sem ele, não há fio extra nem custo no caminho crítico.

**Conclusão honesta:** o firmware removeu o timer da ISR e não introduz espera no caminho crítico; isso é o que o código permite afirmar. **Não é possível afirmar “0 ms” nem “igual ao fio” sem medir** o rádio, o controle e o frame SPI. A medição recomendada está na seção 8.5.

---

## 6. Matriz de validação em hardware (obrigatória antes de release)

### 6.1 Boot e portas

- [ ] Ligar o console **sem** controle BT: as duas portas aceitam controle com fio; o ESP32 não responde como controle.
- [ ] Conectar um controle BT: a porta correspondente passa a responder; o controle com fio da outra porta continua funcionando.
- [ ] Desconectar o controle BT: a porta volta a ficar invisível; o controle com fio volta a funcionar.

### 6.2 DS4 / clone

- [ ] Player 1: 1 piscada azul; player 2: 2 piscadas.
- [ ] Barra cinza no digital e vermelha no analógico.
- [ ] Pulso de conexão de 150 ms.
- [ ] Conectar durante rumble contínuo do jogo: o rumble não é cortado.
- [ ] Clone DS4: report 0x05 no canal de interrupção, sem travar o handshake.

### 6.3 DS5

- [ ] LEDs de player corretos para 1, 2, 3, 4 (e 5, se aplicável).
- [ ] Barra cinza na conexão e vermelha no analógico.
- [ ] Sem blink azul.
- [ ] Pulso de conexão.
- [ ] Rumble do jogo (THPS2/RE3/Alien) com motores LF/HF corretos.
- [ ] Trigger haptic (ACC_RUMBLE/ACC_BOTH).

### 6.4 Switch / 8BitDo

- [ ] LED de player do Switch/8BitDo atualiza.
- [ ] Rumble contínuo não é interrompido por evento de LED.
- [ ] Evento de LED durante rumble: LED muda e rumble continua.

### 6.5 Multitap e memory card

- [ ] Multitap na porta 1 (slots 1–4) e na porta 2 (slots 1–4).
- [ ] LED/barra de cada slot reflete o modo analógico correto.
- [ ] Reset do console com multitap: todos os slots voltam a digital.
- [ ] Memory card fora do escopo desta versão; o foco é o controle.
- [ ] Somente no build opcional `CONFIG_BR_PS1_MEMORY_CARD=y`: cartão virtual no ESP32 principal, salvar/reiniciar/carregar e estresse de gravação com rumble ativo.

### 6.6 Timeout de 3 s

- [ ] Deixar o jogo em pausa/menu por >3 s: o modo analógico **não** cai se a porta 1 estiver ativa.
- [ ] Reset físico do console: as portas voltam a digital.
- [ ] Sem tráfego SPI em ambas as portas: reset após ~3 s.

---

## 7. Build

### 7.1 Pré-requisitos

- ESP-IDF **v5.5** (o projeto usa `idf.py`);
- Python e toolchain do ESP32 instalados;
- submódulo `components/queue_bss/liblfds` (liblfds 7.1.1 do fork `darthcloud/liblfds7.1.1`).

### 7.2 Passos

```bash
# 0) Instalar o ESP-IDF v5.5.1 (uma vez; requer Python 3 e git)
git clone -b v5.5.1 --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf && ./install.sh esp32 && . ./export.sh
cd -

cd v0.6-deepseek

# 1) Popular o submódulo (já populado nesta cópia; o script é idempotente)
./scripts/fetch_liblfds.sh

# 2) Carregar a configuração de hardware do PlayStation
cp configs/hw1/playstation sdkconfig

# 3) Compilar
BR_HW=_hw1 BR_SYS=_playstation idf.py reconfigure build
```

> **Cópia de trabalho vs. pacote limpo:** na cópia de trabalho o diretório chama-se `v0.6-deepseek/`; no pacote limpo, a raiz já é a pasta `BlueRetro-PS1-v0.6-deepseek/`. Nesse caso, **não** execute `cd v0.6-deepseek`; rode os comandos a partir da raiz do pacote.

Ou use o script:

```bash
./build.sh
```

O script `build.sh`:
- popula o liblfds automaticamente se a pasta estiver vazia;
- **sempre** copia `configs/hw1/playstation` para `sdkconfig` (evita usar um `sdkconfig` antigo de outro alvo);
- verifica se `idf.py` está no `PATH`;
- roda `idf.py reconfigure build` com `BR_HW=_hw1 BR_SYS=_playstation`.

### 7.3 Resultado do build e build PS1-only

Ambiente usado: **ESP-IDF v5.5.1**, toolchain `xtensa-esp32-elf` 14.2.0, alvo `esp32`, `CONFIG_COMPILER_OPTIMIZATION_SIZE=y`, `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y`.

| Métrica | Build com todos os sistemas wired | Build PS1-only (v0.6-deepseek) | Ganho |
|---|---:|---:|---:|
| Binário da aplicação | 605.616 B (`0x93db0`) | **550.640 B (`0x866f0`) padrão sem cartão** / **551.984 B (`0x86c30`) com cartão opcional** | **-54.976 B** / **-53.632 B** |
| Flash livre na partição de 1 MB | 42% | **47%** | +5 p.p. |
| IRAM usada | 129.763 B (99,0%; 1.309 B livres) | **106.135 B (81,0%; 24.937 B livres)** | **-23.628 B de IRAM** |
| DRAM usada | 57.452 B (46,1%) | **46.524 B (37,3%; 78.056 B livres)** | **-10.928 B de DRAM** |
| Heap do memory card virtual | 128 KB alocados no boot | **0 no padrão / 128 KB no build opcional com cartão** | **-128 KB de heap** no padrão |

O build PS1-only foi implementado em `main/CMakeLists.txt`:

- quando `CONFIG_BLUERETRO_SYSTEM_PSX_PS2=y`, os fontes dos adaptadores wired não-PlayStation e seus drivers de hardware são removidos da compilação;
- as tabelas de dispatch em `adapter/wired/wired.c`, `wired/wired_bare.c` e `wired/wired_rtos.c` mantêm o índice `WIRED_MAX`, mas os slots dos outros sistemas viram `NULL` via macro `WIRED_OTHER()`;
- `BR_PS1_ONLY=1` é definido para o componente `main`;
- o build universal (outro `CONFIG_BLUERETRO_SYSTEM_*`) mantém a lista de fontes original e os macros `WIRED_OTHER()` expandem para as funções; **esse build não foi recompilado nesta rodada**, mas o condicional foi escrito para não alterar o caminho universal.

**Fontes removidas da compilação no build PS1-only** (lista exata do `main/CMakeLists.txt`):

- adaptadores: `adapter/wired/npiso.c`, `cdi.c`, `saturn.c`, `pce.c`, `genesis.c`, `real.c`, `jag.c`, `pcfx.c`, `jvs.c`, `n64.c`, `dc.c`, `gc.c`, `sea.c`, `wii.c`, `parallel_1p.c`, `parallel_2p.c`;
- drivers: `wired/npiso_io.c`, `cdi_uart.c`, `pce_io.c`, `sega_io.c`, `nsi.c`, `maple.c`, `jvs_uart.c`, `parallel.c`, `pcfx_spi.c`, `real_spi.c`, `snes_spi.c`, `jag_io.c`, `sea_io.c`, `wii_i2c.c`.

**Mantidos no build PS1-only:** `adapter/wired/ps.c` (PS1/PS2), `wired/ps_spi.c`, `wired/detect.c`, `wired/wired_bare.c`, `wired/wired_rtos.c`, `adapter/wired/wired.c`, além de todo o stack BT/HID, wireless, memory card, multitap, config e tools.

**Por que isso foi necessário:** o build completo deixava a **IRAM em 99,0%** (1.309 bytes livres). Como o caminho crítico do PS1 (`ps_spi.c`) usa IRAM, qualquer ajuste futuro poderia estourar a região. O build PS1-only libera ~23,5 KB de IRAM e ~10,8 KB de DRAM; a remoção dos outros sistemas não altera a lógica do adaptador PlayStation (as mudanças de latência no `ps_spi.c` são independentes e estão na seção 3.7).

**O que foi mantido de propósito:** o stack Bluetooth/HID e os adaptadores **wireless** (DS4/DS5, Switch, PS3, Wii, 8BitDo) são **entradas de controle**, não sistemas de console; removê-los quebraria os controles. O adaptador **PS2** compartilha `ps.c`/`ps_spi.c` com o PS1 (mesma família de protocolo) e também foi mantido; o corte é apenas dos **sistemas wired de outros consoles**. O `detect.c` e a tabela `sys_name` ainda conhecem os IDs dos outros sistemas (código minúsculo, sem drivers), mas as entradas de dispatch estão em `NULL`.

**Validação estática do binário PS1-only:** `ps_spi_init`, `ps_spi_poll`, `ps_spi_port_cfg`, `ps_meta_init`, `ps_init_buffer`, `ps_from_generic` e `ps_fb_to_generic` estão presentes no ELF; os símbolos dos adaptadores removidos (`nsi_init`, `maple_init`, `sega_io_init`, `snes_spi_init`, `npiso_init`, `pce_io_init`, etc.) estão ausentes (`nm`: 0 ocorrências).

**Artefato padrão (sem cartão virtual):** `build/BlueRetro_hw1_playstation.bin`, **550.640 bytes** (`0x866f0`), SHA-256 `af8dc6fb96777ec57ee518c4bfc5fba4523dfa2e1049ff52c5fe4f7aa12c169b`. O `build/` também contém `bootloader/bootloader.bin` (23.840 B, `0x5d20`), `partition_table/partition-table.bin` (3.072 B), `ota_data_initial.bin` (8.192 B) e `flash_args`.

**Build opcional com cartão virtual:** `BlueRetro_hw1_playstation_mc.bin`, **551.984 bytes** (`0x86c30`), SHA-256 `563f4294eafcc1505b028fae6fbe43db9c746164c20b77f5a2106b918ade01e1` (pode variar por timestamp/caminho). Ele mantém blink, pulso e cores; liga o cartão virtual e consome **128 KB de heap DMA**. O build padrão **não** o inclui porque o MC está fora do escopo desta versão; o foco é o controle.

**Menu Kconfig `PS1 Custom Edition`:** o build padrão mantém exatamente o comportamento personalizado (blink N, pulso de 150 ms, vermelho `0x6D`, cinza `0x101010`) e deixa o **cartão virtual desligado**. As opções `BR_PS1_BATTERY_IND`, `BR_PS1_LATENCY_STATS`, `BR_PS1_LATENCY_PROBE` e `BR_PS1_NIGHT_MODE` vêm **desligadas** por padrão; o código foi compilado nas variantes de teste, mas ainda não foi homologado em hardware. Para um build standalone com cartão: `CONFIG_BR_PS1_MEMORY_CARD=y`.

**Variante de teste com todas as opções:** compilou com `BR_PS1_BLINK=n`, `BR_PS1_CONN_RUMBLE=n`, `BR_PS1_NIGHT_MODE=y`, `BR_PS1_MEMORY_CARD=n`, `BR_PS1_BATTERY_IND=y`, `BR_PS1_LATENCY_STATS=y` e `BR_PS1_LATENCY_PROBE=y` (GPIO 25): **`0x869b0` (551.344 B)**. Isso prova que os caminhos condicionais compilam; não é a variante entregue.

**O que falta:** executar a matriz de hardware da seção 6. O build prova compilação e layout, **não** prova operação no console.

**Nenhuma alteração deve ser considerada homologada sem a matriz da seção 6.**

### 7.4 Pacote para os devs

A pasta `v0.6-deepseek/` é **autocontida para compilar**: inclui `main/`, `components/` (com o `liblfds` populado), `configs/`, `scripts/`, `build.sh`, `sdkconfig.defaults`, `partitions.csv`, `README.md`, esta análise e o `build/` com o binário. Se ela for compactada inteira, **o binário e a documentação vão junto**.

Porém, o `build/` tem ~134 MB de objetos intermediários e caminhos absolutos da máquina que compilou. Para enviar aos devs, o ideal é um pacote limpo:

```bash
./scripts/package-release.sh
```

O script gera, em `dist/` (fora de `v0.6-deepseek/`):

- `BlueRetro-PS1-v0.6-deepseek/` — fonte completa + `README.md` + esta análise + `COMPARATIVO-v0.5.1-v0.6-deepseek.md` + `LICENSE` + `scripts/` + `configs/` + `main/` + `components/` + `tools/` + `static/` + `tests/` + `patches/`;
- `firmware/` dentro do pacote, com o `.bin` padrão **sem cartão virtual**, a variante `_mc` opcional (se gerada), bootloader, tabela de partições, `ota_data_initial.bin`, `flash_args` e `LEIA-ME.txt`;
- `patches/v0.6-deepseek.patch` + `patches/README.md` (patch formal contra a v0.5.1);
- `COMPARATIVO-v0.5.1-v0.6-deepseek.md` com o antes/depois e o motivo de cada mudança;
- `PACOTE-PARA-DEVS.md` com hash, tamanho, comando de gravação e o que ainda falta validar;
- arquivos `.tar.gz` e `.zip` em `dist/`.

O `sdkconfig` gerado e o `build/` **não** entram no pacote limpo; o `build.sh` recria a configuração a partir de `configs/hw1/playstation` e de `sdkconfig.defaults`. O `liblfds` populado entra, então o dev não precisa baixar o submódulo para compilar. O `scripts/package-release.sh` é uma ferramenta da cópia de trabalho e **não** é copiado para dentro do pacote limpo (para não ser executado sem `build/`).

**Pacote limpo testado:** o `.zip` foi descompactado em um diretório temporário e compilado do zero com ESP-IDF v5.5.1; o `build.sh` terminou com **`0x866f0` (550.640 B, 47% livre)**. O hash do binário recompilado pode diferir do artefato entregue por causa de timestamp/caminho de build, mas o tamanho é o mesmo.

**O que não está no pacote:** o código original v0.5.1 (o patch formal cobre o diff; o pacote é a árvore final), o ESP-IDF (instale o v5.5.1) e os resultados de hardware, que ainda não existem. O `dist/` é **fora** de `v0.6-deepseek/`; se você compactar apenas `v0.6-deepseek/`, os `.zip`/`.tar.gz` do `dist/` não vão junto (o `build/` e o binário vão).

---

## 8. Pendências e riscos residuais

### 8.1 Teste em hardware (bloqueante)

O build foi concluído com ESP-IDF v5.5.1 (seção 7.3) e o binário PS1-only foi gerado. O que ainda falta é **executar a matriz da seção 6 no PS1 real**: boot sem controle, duas portas com fio, DS4/DS5, multitap, memory card, reconexão, blink/pulso, LED vermelho e medição de latência. Compilar não substitui essa etapa.

### 8.2 Clone 0x05 no canal de interrupção

O fork envia o report 0x05 do clone **somente** no canal de interrupção, para evitar flood/handshake no canal de controle. A v0.6 mantém essa decisão e adiciona o lock. **Testar com clones reais** (não apenas DS4 oficial): conexão, LED, rumble e reconexão.

### 8.3 `bt_hci_pkt_tmp` — risco residual

A v0.6 separa o HID e serializa HCI/ATT nos limites de tarefa (RX, SDP, inquiry/disconnect). Ainda existem comandos HCI disparados por tarefas externas que montam em `bt_hci_pkt_tmp.cp` antes de chamar `bt_hci_cmd()`; para eliminar 100% do risco, o próximo passo é:

- migrar todos os construtores HCI/ATT para um buffer por contexto (ou um pool com lock cobrindo build + enqueue);
- ou concentrar todos os envios HCI em uma única tarefa/queue.

Isso é uma refatoração maior, não necessária para o caminho de feedback que causava o CRC corrompido, mas recomendada para robustez de longo prazo.

**Tradeoff introduzido:** `bt_host_txq_add()` agora espera no máximo 10 ms e **descarta** o pacote se a fila continuar cheia. Isso evita o deadlock descrito em 3.2, mas significa que, sob saturação extrema do controlador, um relatório HID ou uma resposta HCI pode ser perdido. O feedback HID é reenviado no próximo tick; comandos HCI críticos (conexão/SDP) devem ser validados na matriz de hardware.

### 8.4 DS5 com mais de 5 jogadores

O DS5 só tem 5 LEDs de player. A v0.6 limita jogadores 6–7 ao padrão de 5 LEDs (`0x1F`). Se o time quiser outra convenção (ex.: piscar), é uma decisão de produto.

### 8.5 Latência do caminho com fio (timer removido; medição pendente)

A v0.6 **não lê timer na ISR**. O `packet_end()` incrementa um contador de 32 bits e o `ps_spi_poll()` carimba o CCOUNT do core 0 quando vê o contador mudar (seção 3.7). Isso remove o custo de `esp_timer_get_time()` do core 1 e elimina o risco de chamada em flash com cache desabilitado.

O que ainda pode adicionar latência ao **input** (do controle BT até o próximo frame SPI) e deve ser medido em hardware:

- contenção do `bt_host_tx_lock` entre a tarefa de feedback (core 0) e o processamento RX; o lock é mantido apenas durante build + enqueue e, no caminho normal, o enqueue não bloqueia;
- `bt_host_txq_add()` espera no máximo 10 ms **somente** quando a fila TX está cheia; em operação normal o caminho é não bloqueante;
- o polling do SPI e o escalonamento da `sys_mgr_task` continuam iguais aos da base; a ISR apenas sinaliza.

**Medição recomendada:** usar o teste de timing já existente (`CONFIG_BLUERETRO_BT_TIMING_TESTS`) e/ou o probe novo (`CONFIG_BR_PS1_LATENCY_PROBE`) para comparar com a v0.5.1 e com o controle com fio. O probe baixa no report HID recebido e sobe no fim do frame SPI; com o `CONFIG_BR_PS1_LATENCY_STATS` ligado, o serial mostra frames, idle resets, drops da fila e a maior espera do lock. O pino do probe é escolhido no Kconfig e **não** usa o barramento PS1; a opção vem desligada por padrão. Sem essa medição, não afirmar “0 ms”; afirmar que o caminho crítico não foi onerado.

### 8.6 README e CI

O README foi atualizado para:

- reconhecer as quatro personalizações e o comportamento do DS5;
- remover a alegação de “20 ms sincronizado a frames de vídeo” (o keepalive é 20 ms, não é sincronizado ao vídeo);
- remover a alegação de que `0x43` é “pause” (o firmware não usa `0x43` como gatilho de pausa);
- suavizar a alegação de watchdog “200–250 ms” (sem fonte oficial);
- remover a alegação de CI GitHub Actions (não há `.github` neste snapshot);
- trocar “Controles Homologados” por “Controles Suportados” e declarar explicitamente que a v0.6 **foi compilada, mas ainda não foi testada em hardware** neste ambiente;
- documentar `scripts/fetch_liblfds.sh`, `build.sh` e `.gitmodules`.

### 8.7 Timers de init one-shot e reconexão rápida

Os timers de init do DS4, PS3 e Switch são `esp_timer` one-shot. O `bt_host_reset_dev()` **não** os deleta, para não liberar um timer cujo callback pode estar em execução. Se o controle desconectar antes do disparo e reconectar em menos de 1 s, o timer antigo pode disparar já no dispositivo novo (o `ids.type` volta a `BT_PS`/`BT_PS3`/`BT_SW`) e antecipar/pular o init. Impacto observável: blink/pulso duplicado e um timer alocado; não é crash. Correção definitiva: passar um contexto com `handle` + geração para o callback e comparar a geração antes de agir.

---

## 9. Arquivos alterados na v0.6-deepseek

| Arquivo | Mudança |
|---|---|
| `main/Kconfig.projbuild` | Menu **PS1 Custom Edition**: blink, pulso, cores, night mode, memory card (padrão `n`), bateria, estatísticas e probe de latência. |
| `main/bluetooth/hidp/ps.h` | Layout DS5 de 77 bytes; `led_brightness`; helper de player LED; `bt_hid_ps_tick`; helpers de cor/brilho (`ps1_led_color`, `ps1_led_brightness`, `ps1_blink_brightness_max`). |
| `main/bluetooth/hidp/ps.c` | Flags DS5; player LEDs; blink/pulso sem `esp_timer`; save/restore de motores; `INIT_DONE` no fim; guarda do timer de init; callback de init sob lock + checagem `BT_PS`; lock do clone; cores Kconfig/night; indicador de bateria baixa (opcional). |
| `main/bluetooth/hidp/hidp.h` / `hidp.c` | `bt_hid_pkt_tmp` dedicado. |
| `main/bluetooth/hidp/generic.c`, `ps3.c`, `wii.c`, `sw.c` | Uso do buffer HID dedicado; lock/two-phase no Switch; callbacks de init do PS3/Switch sob lock. |
| `main/adapter/wireless/ps.c` | Flags DS5 no feedback; player LEDs; cores Kconfig/night; brilho DS5 configurável. |
| `main/adapter/memory_card.c` | `CONFIG_BR_PS1_MEMORY_CARD`: compila o cartão virtual ou stubs sem alocar os 128 KB; padrão desligado (MC fora do escopo). |
| `configs/hw1/playstation` | `CONFIG_BR_PS1_MEMORY_CARD=n` explícito; o build padrão não aloca o cartão virtual. |
| `main/adapter/wireless/sw.c` | Marcador `output[127]` em todo feedback. |
| `main/wired/ps_spi.c` | contador `pkt_cnt` + CCOUNT do core 0 no poll (ISR sem timer); `mt_first_port`; reset 0x73/0x79/0xF3; `ps_dev_id_is_analog`; contadores de latência e GPIO de probe opcional. |
| `main/CMakeLists.txt` | Build PS1-only: remove adaptadores/drivers wired não-PlayStation quando `CONFIG_BLUERETRO_SYSTEM_PSX_PS2=y`; define `BR_PS1_ONLY`. |
| `main/adapter/wired/wired.c` | Tabelas de dispatch com slots não-PS1 em `NULL` no build PS1-only; `wired_para_turbo_mask_hdlr` sem os ramos paralelo/SEA. |
| `main/wired/wired_bare.c` | Tabelas `wired_init`/`wired_port_cfg` com slots não-PS1 em `NULL`; includes condicionais. |
| `main/wired/wired_rtos.c` | `wired_rtos_init()` vazio no build PS1-only (paralelo/SEA não compilados). |
| `main/system/manager.c` | `err_led_set` restrito ao bloco `CONFIG_BLUERETRO_HW2` para build sem warning no alvo hw1. |
| `main/bluetooth/host.c` | `bt_hid_ps_tick` sob lock; sweep de MC; lock no RX/SDP e no reset; `bt_host_txq_add` com espera limitada de 10 ms; contadores de fila cheia/espera do lock e probe de latência no `bt_host_bridge`. |
| `main/bluetooth/host.h` | Getters das estatísticas de fila/lock. |
| `main/bluetooth/hci.c` | Lock em inquiry/disconnect. |
| `main/adapter/adapter.c` | Comentário sobre recuperação de `FB_TYPE_MEM_WRITE`. |
| `README.md` | Personalizações, Kconfig, PS2/memory card, patch e pacote. |
| `ANALISE-v0.6-deepseek.md` | Este documento. |
| `COMPARATIVO-v0.5.1-v0.6-deepseek.md` | Antes/depois e motivo de cada mudança (documento para os devs). |
| `PACOTE-PARA-DEVS.md` | Resumo do pacote, hashes, variantes e gravação. |
| `patches/README.md`, `patches/v0.6-deepseek.patch` | Patch formal contra a v0.5.1 e instruções de aplicação. |
| `scripts/fetch_liblfds.sh`, `scripts/package-release.sh`, `build.sh`, `.gitmodules` | Reprodutibilidade do build e empacotamento limpo. |

### 9.1 Duplicidades, fronteiras e não extrapolação de hardware

| Item | Situação na v0.6-deepseek |
|---|---|
| Buffers HID vs. HCI | `bt_hid_pkt_tmp` e `bt_hci_pkt_tmp` são **distintos**; não há buffer compartilhado entre feedback HID e HCI/ATT. O `bt_host_tx_lock` serializa o uso. |
| Feedback DS4/DS5 no caminho BT e no wired | `main/bluetooth/hidp/ps.c` monta o relatório HID; `main/adapter/wireless/ps.c` preenche o mesmo layout para o feedback vindo do jogo. O helper `bt_hidp_ps5_player_led()` é único em `ps.h`; as flags DS5 são repetidas nos dois caminhos porque são dois pontos de entrada, não duas implementações concorrentes. |
| Tabelas `wired.c` vs. `wired_bare.c` | São camadas diferentes: `wired.c` mapeia `dev_mode` → parser/feedback; `wired_bare.c` mapeia `system_id` → init/port_cfg de hardware. Ambas foram condicionadas no build PS1-only; nenhuma foi duplicada. |
| Blink e pulso de conexão | Duas máquinas de estado separadas (`ps4_blink_ctx` e `ps_conn_rumble_ctx`), ambas acionadas pelo mesmo tick de 10 ms; não usam `esp_timer`. |
| PS1 vs. PS2 | `ps.c`/`ps_spi.c` são compartilhados pela família PlayStation; o PS2 não foi removido porque o código é comum e o corte exigiria separar protocolos. O build é PS1-only nos **outros consoles wired**. |
| Controles wireless | O stack BT/HID e os adaptadores wireless são **entradas**, não sistemas de console; foram mantidos para não perder DS4/DS5/Switch/PS3/Wii/8BitDo. |
| `detect.c` / `sys_name` | Ainda conhecem os IDs de todos os sistemas (metadados), mas os drivers e as entradas de dispatch dos outros consoles foram removidos; não há código de hardware não-PS1 no binário. |
| Timeout de inatividade | Um único contador `pkt_cnt` + um único `last_spi_ccnt` por porta; não há segundo timestamp nem `esp_timer` no caminho SPI. |

**Não extrapolação de hardware:** a v0.6-deepseek **não adiciona, remove ou reconfigura GPIO, tensão, clock de periférico ou temporizador de hardware no build padrão**. O build PS1-only apenas deixa de compilar drivers de outros consoles; o `ps_spi`, o `detect`, o `wired_bare` e a pinagem do hw1 permanecem. O timeout usa o CCOUNT da CPU já existente; o `manager.c` só condiciona uma variável de LED ao bloco `CONFIG_BLUERETRO_HW2`. A única adição opcional de GPIO é o **probe de latência**, que fica desligado por padrão e usa um pino livre escolhido no Kconfig; ele não toca o barramento PS1. A única mudança de timing no build padrão é a remoção do timer da ISR, que **reduz** o trabalho no core 1.

---

## 10. Conclusão

As quatro personalizações da v0.5.1 são **coerentes, intencionais e devem ser preservadas**. O que a v0.5.1 tinha de realmente problemático era:

1. o layout/flags do DualSense;
2. a concorrência do buffer HID/HCI;
3. a regressão do marcador do Switch/8BitDo;
4. os timers auto-destrutivos;
5. a perda de flush do memory card;
6. o mapeamento de multitap e o reset de 0x79/0xF3;
7. o CCOUNT entre núcleos;
8. o risco de deadlock da fila de TX (espera infinita sob lock), agora com espera limitada.

A v0.6-deepseek corrige esses pontos, documenta o que permanece como risco residual e entrega o comportamento do DS5 solicitado (LEDs de player + barra cinza/vermelha). O build padrão com ESP-IDF v5.5.1 foi concluído em **550.640 B (`0x866f0`) sem o cartão virtual**, liberando **128 KB de heap DMA**; o build opcional com cartão fica em **551.984 B (`0x86c30`)** (seção 7.3). O menu Kconfig permite ligar/desligar os recursos sem editar código; o patch formal está em `patches/v0.6-deepseek.patch` e o comparativo para os devs em `COMPARATIVO-v0.5.1-v0.6-deepseek.md`.

O próximo passo é **executar a matriz de validação da seção 6 em um PS1 real**, confirmar os offsets de bateria e o pino do probe, e medir a latência do caminho com fio com o instrumento descrito na seção 8.5. A página Web Bluetooth para as opções novas é uma fase 2 separada (seção 3.9) e não altera o firmware atual.

---

### Apêndice A — Como o DS4 e o DS5 se diferenciam

| Recurso | DS4 / clone | DS5 |
|---|---|---|
| Identificação de player | Blink azul N vezes + LED da barra | LEDs de player do próprio DS5 |
| Barra na conexão | Cinza `0x101010` (ou vermelha se já estiver em analógico) | Cinza `0x101010` (ou vermelha se já estiver em analógico) |
| Barra no analógico | Vermelha `0x00006D` | Vermelha `0x00006D` |
| Pulso de conexão | 150 ms no motor leve | 150 ms no motor leve |
| Report de feedback | 0x11 (PS4) / 0x05 (clone) | 0x31 (PS5) |
| Flags de LED | `conf0 = 0xC4`, `conf1 = 0x03` | `conf1 = 0x14`, `valid_flag2 = 0x02`, `lightbar_setup = 0x02`, `led_brightness = 0x02` |

### Apêndice B — Glossário rápido

- **LF/HF:** motor de baixa frequência (pesado) / alta frequência (leve).
- **`out_idx`:** índice de saída (porta/slot) do controle no adapter; é o que define o número do player.
- **`pids->id`:** id interno do dispositivo Bluetooth (0..6), usado para indexar `bt_adapter.data[]` e os contextos de blink.
- **`FB_TYPE_RUMBLE`:** evento de rumble; valor 0.
- **`FB_TYPE_STATUS_LED`:** evento de LED de status (analógico/digital).
- **`FB_TYPE_PLAYER_LED`:** evento de LED de identificação de player.
- **`FB_TYPE_MEM_WRITE`:** evento que sinaliza bloco sujo do memory card virtual.
