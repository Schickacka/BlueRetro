# BlueRetro PS1 Custom Edition — pacote para os devs

> **Documento histórico da v0.6.** Não reflete a v0.7.4. Para o estado atual,
> veja `README.md` e a release mais recente no GitHub.

**Versão:** v0.6-deepseek  
**Base:** BlueRetro v0.5.1, fork PlayStation 1 Custom Edition  
**Data do build:** 2026-10-20  
**Ambiente:** ESP-IDF v5.5.1, toolchain `xtensa-esp32-elf` 14.2.0, alvo ESP32

## Artefatos

| Item | Valor |
|---|---|
| Aplicação padrão (sem cartão virtual) | `firmware/BlueRetro_hw1_playstation.bin` |
| Tamanho padrão | **550.640 bytes** (`0x866f0`) |
| SHA-256 padrão | `af8dc6fb96777ec57ee518c4bfc5fba4523dfa2e1049ff52c5fe4f7aa12c169b` |
| Build opcional com cartão virtual | `firmware/BlueRetro_hw1_playstation_mc.bin` (se gerado) |
| Tamanho com cartão | **551.984 bytes** (`0x86c30`) |
| SHA-256 com cartão | `563f4294eafcc1505b028fae6fbe43db9c746164c20b77f5a2106b918ade01e1` |
| Flash livre | **47%** da partição de 1 MB |
| IRAM (padrão) | **106.135 B usados; 24.937 B livres (19,0%)** |
| DRAM (padrão) | **46.524 B usados; 78.056 B livres (62,7%)** |
| Heap do cartão virtual | **0 no padrão; 128 KB no build opcional com cartão** |

O build padrão **não** emula o memory card. O MC está fora do escopo desta versão; o foco é o controle. Desligar o cartão virtual libera **128 KB de heap DMA** e ~1,3 KB de flash no ESP32 principal. O código do cartão virtual e o fix de flush continuam no repositório para um build standalone opcional (`CONFIG_BR_PS1_MEMORY_CARD=y`).

O pacote inclui bootloader, tabela de partições, `ota_data_initial.bin` e `flash_args` para a aplicação padrão.

## Conteúdo

- `main/`, `components/` (com o `liblfds` 7.1.1 populado), `configs/`, `scripts/`, `tools/`, `static/`, `tests/`;
- `CMakeLists.txt`, `sdkconfig.defaults`, `partitions.csv`, `pytest.ini`, `build.sh`, `.gitmodules`, `LICENSE`;
- `README.md`, `ANALISE-v0.6-deepseek.md` e `COMPARATIVO-v0.5.1-v0.6-deepseek.md`;
- `patches/v0.6-deepseek.patch` + `patches/README.md` (patch formal contra a v0.5.1);
- `firmware/` com a aplicação padrão (sem cartão), a variante `_mc` opcional (se gerada), bootloader, tabela de partições, `ota_data_initial.bin`, `flash_args` e `LEIA-ME.txt`;
- este `PACOTE-PARA-DEVS.md`.

O `build/` intermediário **não** vai no pacote limpo. O `sdkconfig` é recriado pelo `build.sh` a partir de `configs/hw1/playstation` e de `sdkconfig.defaults`.

## Gravar o firmware

A partir da pasta `firmware/`:

```bash
python -m esptool --chip esp32 -p /dev/ttyUSB0 -b 460800 \
  --before default_reset --after hard_reset write_flash @flash_args
```

No Windows, troque `/dev/ttyUSB0` por `COMx`. O `flash_args` grava:

- bootloader em `0x1000`;
- tabela de partições em `0x8000`;
- `ota_data_initial.bin` em `0xd000`;
- aplicação padrão em `0x10000`.

Se a variante `_mc` estiver presente e você quiser o cartão virtual no ESP32 principal:

```bash
python -m esptool --chip esp32 -p /dev/ttyUSB0 -b 460800 \
  --before default_reset --after hard_reset write_flash \
  0x1000 bootloader/bootloader.bin \
  0x8000 partition_table/partition-table.bin \
  0xd000 ota_data_initial.bin \
  0x10000 BlueRetro_hw1_playstation_mc.bin
```

## Compilar a partir do fonte

Requisitos: ESP-IDF **v5.5.1** e Python 3.

```bash
# Na raiz do pacote
. ~/esp/esp-idf/export.sh
./build.sh
```

O `build.sh` copia `configs/hw1/playstation` para `sdkconfig` e executa `idf.py reconfigure build`. O resultado fica em `build/BlueRetro_hw1_playstation.bin`.

O build é **PS1-only** quando `CONFIG_BLUERETRO_SYSTEM_PSX_PS2=y`: os adaptadores/drivers wired de outros consoles não são compilados, e as tabelas de dispatch mantêm o índice `WIRED_MAX` com os slots removidos em `NULL`.

### Kconfig PS1 Custom

O menu **Kconfig → PS1 Custom Edition** (`main/Kconfig.projbuild`) controla:

- `BR_PS1_BLINK` (padrão `y`);
- `BR_PS1_CONN_RUMBLE` (padrão `y`);
- `BR_PS1_LED_ANALOG_COLOR` (padrão `0x00006D`);
- `BR_PS1_LED_DIGITAL_COLOR` (padrão `0x00909194`);
- `BR_PS1_LED_DIGITAL_COLOR_DS5` (padrão `0x000000`);
- `BR_PS1_NIGHT_MODE` (padrão `n`);
- `BR_PS1_MEMORY_CARD` (padrão `n`; o MC está fora do escopo desta versão; ligar só em build standalone);
- `BR_PS1_BATTERY_IND` (padrão `n`);
- `BR_INACTIVITY_TIMEOUT_MIN` (padrão `10`; `0` desativa o auto power-off).
- `BR_PS1_LATENCY_STATS` (padrão `n`);
- `BR_PS1_LATENCY_PROBE` / `BR_PS1_LATENCY_PROBE_GPIO` (padrão `n` / `26`).

Para ativar, acrescente a linha no `configs/hw1/playstation` (com uma quebra de linha antes, porque o arquivo pode não terminar em newline) e rode `./build.sh`:

```bash
printf '\nCONFIG_BR_PS1_BATTERY_IND=y\n' >> configs/hw1/playstation
./build.sh
```

A bateria e o probe vêm desligados de propósito: os offsets de bateria precisam de confirmação no hardware e o probe exige um pino livre (não use um pino do barramento PS1).

## Personalizações preservadas

1. ESP32 invisível ao console quando não há controle Bluetooth conectado (`hotplug = 1`).
2. Emulação 1:1 do controle com fio: deadzone LF `0x40`, saturação HF `0xFF`, sem gate `acc_mode & ACC_RUMBLE` no comando `0x42`.
3. LED azul N piscadas por porta (DS4/clones) + vibração de conexão de 150 ms.
4. LED vermelho no modo analógico (`0x6D0000`) e cinza no digital (`0x101010`), configuráveis no Kconfig.
5. DualSense: LEDs de player do próprio controle, barra cinza/vermelha, sem blink azul, pulso de conexão mantido.
6. Memory card: fora do escopo desta versão; o foco é o controle; o build padrão libera 128 KB de heap.

## PS2, memory card e página web

- **PS2:** o controle compartilha `ps.c`/`ps_spi.c` com o PS1 e o código detecta `DEV_PS2_DS2`/`DEV_PS2_MULTITAP`. O controle tende a funcionar com a mesma fiação, mas não foi homologado. O memory card PS2 de 8 MB **não** está implementado neste firmware; o MC está fora do escopo desta versão; o foco é o controle. O PS2 foi mantido por ser código compartilhado e o custo de removê-lo não compensar.
- **Memory card:** o build padrão **não** emula o cartão PS1 no ESP32 principal; o memory card está fora do escopo desta versão; o foco é o controle. O `BR_PS1_MEMORY_CARD` pode ser ligado em um build standalone, mas consome 128 KB de heap. O “pulso de gravação” não foi implementado; só faria sentido com o cartão virtual habilitado.
- **Página web:** o firmware já tem o serviço de configuração BLE (`att_cfg.c`), usado pelo web app oficial via Web Bluetooth. O HTML não está neste repositório; o `main/tests/ws_srv.c` é um servidor de teste via Ethernet. Uma página Web Bluetooth separada para as opções novas é fase 2; não há Wi-Fi/HTTP no firmware de produção. Desligar o cartão virtual libera heap para o stack BT/config.

## Teste do pacote limpo

O `.zip` foi descompactado em um diretório temporário e compilado do zero com ESP-IDF v5.5.1; o `build.sh` terminou com **`0x866f0` (550.640 B, 47% livre)**. O hash do binário recompilado pode diferir do artefato entregue por causa de timestamp/caminho de build; o tamanho é o mesmo.

## O que ainda falta validar em hardware

O build prova compilação e layout; **não** prova operação no PS1. A matriz obrigatória está na seção 6 de `ANALISE-v0.6-deepseek.md`:

- boot sem controle BT e duas portas com fio;
- DS4, clone DS4 e DS5;
- Switch/8BitDo;
- multitap;
- memory card fora do escopo e, opcionalmente, o cartão virtual no ESP32 principal;
- reconexão, blink/pulso, LED vermelho/cinza, timeout de 3 s;
- bateria baixa (confirmar offsets `data[30]`/`data[53]` e o bit de carga);
- night mode;
- estatísticas e probe de latência com um pino livre;
- medição de latência do controle BT até o frame SPI, comparando com a v0.5.1 e com o controle com fio.

**Não há homologação declarada.** O termo “latência quase zero” não pode ser comprovado sem essa medição.

## Riscos residuais documentados

- timers de init one-shot em reconexão muito rápida (DS4/PS3/Switch);
- clone `0x05` no canal de interrupção;
- descarte de pacote se a fila TX saturar por mais de 10 ms;
- DS5 com 6–7 jogadores usa o padrão máximo de 5 LEDs;
- comandos HCI disparados por tarefas externas ainda usam `bt_hci_pkt_tmp` (seção 8.3);
- o cartão virtual está desligado no padrão; se for habilitado, custa 128 KB de heap;
- a página Web Bluetooth e o modo diagnóstico são fase 2.

## Documentos principais

- `ANALISE-v0.6-deepseek.md` — auditoria completa, matriz, riscos e decisões.
- `COMPARATIVO-v0.5.1-v0.6-deepseek.md` — antes/depois e motivo de cada mudança (para os devs).
- `patches/README.md` — como aplicar o patch formal.
